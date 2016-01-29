/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2012      Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <errno.h>

#include <gdk/gdkkeysyms.h>
#include <stdlib.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-browser.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-dnd.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-protected-manager.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-device-monitor.h>
#include <thunar/thunar-protected-chooser-button.h>
#include <thunar/thunar-stock.h>
#include <glib.h>


/* Identifiers for signals */
enum
{
  LIST_UPDATED,
  LAST_SIGNAL,
};

static void           thunar_protected_manager_finalize                     (GObject                  *object);

struct _ThunarProtectedManagerClass
{
  GObjectClass            __parent__;
};

struct _ThunarProtectedManager
{
  GObject                 __parent__;

  GHashTable             *protected_files; /* the currently protected files and their allowed profiles */
  GList                  *file_order;      /* an ordered list of protected files to maintain a consistent order across policy file writes */
  gboolean                policy_loaded;   /* whether protected_files has already been filled with a policy */
  gboolean                policy_dirty;    /* whether protected_files needs to be written to disk */
};

static guint manager_signals[LAST_SIGNAL];


G_DEFINE_TYPE_WITH_CODE (ThunarProtectedManager, thunar_protected_manager, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_BROWSER, NULL))


static void
thunar_protected_manager_class_init (ThunarProtectedManagerClass *klass)
{
  GObjectClass     *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_protected_manager_finalize;

  /**
   * ThunarProtectedManager:list-updated:
   *
   * Invoked whenever the list is re-parsed.
   **/
  manager_signals[LIST_UPDATED] =
    g_signal_new (I_("list-updated"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0, G_TYPE_NONE);
}


static void
thunar_protected_show_protection_dialog_cb (GtkDialog *dialog,
                                            gint       response_id,
                                            gpointer   user_data);

static void
thunar_protected_show_protection_dialog_cb (GtkDialog *dialog,
                                            gint       response_id,
                                            gpointer   user_data)
{
  ThunarProtectedManager *manager  = thunar_protected_manager_get ();
  ProtectionDialogData   *data     = (ProtectionDialogData *) user_data;
  GtkComboBoxText        *pbox;
  GAppInfo               *handler;
  GList                  *lh, *lp;
  const gchar            *handler_exec;
  gchar                  *tmp, *policy = NULL, *profile;

  g_return_if_fail (manager != NULL);
  g_return_if_fail (data != NULL);

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      for (lp = data->profiles, lh = data->handlers, policy = NULL; lp && lh; lp = lp->next, lh = lh->next)
        {
          pbox = lp->data;
          profile = gtk_combo_box_text_get_active_text (pbox);
          handler = thunar_protected_chooser_button_get_current_info (lh->data);

          if (handler && profile)
            {
              handler_exec = g_app_info_get_executable (handler);

              //TODO FIXME register path to desktop file instead.
              tmp = policy;
              if (tmp)
                policy = g_strdup_printf ("%s///%s:%s", tmp, profile, handler_exec);
              else
                policy = g_strdup_printf ("%s:%s", profile, handler_exec);

              if (tmp)
                g_free (tmp);
            }

          if (profile)
            g_free (profile);
          if (handler)
            g_object_unref (handler);
        }

      if (policy)
        {
          for (lp = data->files; lp; lp = lp->next)
          {
            thunar_protected_add_protected_file (lp->data, policy);
          }
          g_free (policy);
        }

      thunar_protected_manager_flush ();
    }

  g_list_free_full (data->files, g_object_unref);
  g_list_free (data->apps); // we should use free_full but there's a referencing bug in the apps ctor
  g_list_free (data->handlers); // destroyed by gtk_widget_destroy on container
  g_list_free (data->profiles); // destroyed by gtk_widget_destroy on container

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

typedef struct _ProfileModelActivateEqualData
{
  const gchar *binary;
  GtkComboBox *box;
} ProfileModelActivateEqualData;

static gboolean
profile_model_activate_equal (GtkTreeModel *model,
                              GtkTreePath *path,
                              GtkTreeIter *iter,
                              gpointer data)
{
  const gchar *binary = ((ProfileModelActivateEqualData *) data)->binary;
  GtkComboBox *box    = ((ProfileModelActivateEqualData *) data)->box;
  gchar       *profile;

  gtk_tree_model_get (model, iter, 0, &profile, -1);

  if (g_strcmp0 (profile, binary) == 0 || (g_strcmp0 (profile, EXECHELP_DEFAULT_USER_PROFILE) == 0 && g_strcmp0 ("exo-open", binary) == 0))
  {
    gtk_combo_box_set_active_iter (box, iter);
    g_free (profile);
    return TRUE;
  }

  g_free (profile);
  return FALSE;
}

static void
thunar_protected_dialog_handler_changed_cb (GtkComboBox *widget,
                                            gpointer     user_data)
{
  GtkComboBox                  *profile = (GtkComboBox *) user_data;
  GtkTreeModel                 *button_model;
  GtkTreeModel                 *profile_model;
  GtkTreeIter                   iter;
  GAppInfo                     *application;
  const gchar                  *exec, *last_name;
  ProfileModelActivateEqualData data;

  button_model = gtk_combo_box_get_model (widget);
  profile_model = gtk_combo_box_get_model (profile);

  if (gtk_combo_box_get_active_iter (widget, &iter))
    {
      gtk_tree_model_get (button_model, &iter,
                          THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_APPLICATION,
                          &application, -1);

      if (application == NULL)
        return;

      exec = g_app_info_get_executable (application);
      last_name = (strrchr (exec, '/'));

      data.box = profile;
      data.binary = last_name ? last_name+1 : exec;

      gtk_tree_model_foreach (profile_model, profile_model_activate_equal, &data);
      g_object_unref (application);
    }
}

static void
thunar_protected_manager_populate_profile_box (GtkWidget *widget)
{
  GtkComboBoxText  *box   = (GtkComboBoxText *) widget;
  GError           *error = NULL;
  GList            *profiles = NULL, *lp;
  GDir             *dir;
  const gchar      *name;
  gchar            *copy;

  //TODO READ home TOO
	// look for a profile in ~/.config/firejail directory
  // if (asprintf(&usercfgdir, "%s/.config/firejail", cfg.homedir) == -1)

  /* read the profile directory */
  dir = g_dir_open ("/etc/firejail", 0, &error);
  if (error)
    {
      TRACE ("Error: could not open \"/etc/firejail\" (%s)", error->message);
      g_error_free (error);
    }

  errno = 0;
  while ((name = g_dir_read_name (dir)) != NULL)
    {
      if (g_str_has_suffix (name, ".profile") &&
          g_strcmp0 (EXECHELP_DEFAULT_USER_PROFILE ".profile", name) &&
          g_strcmp0 (EXECHELP_DEFAULT_ROOT_PROFILE ".profile", name))
        {
          copy = g_strndup (name, strstr (name, ".profile") - name);
          profiles = g_list_prepend (profiles, copy);
        }
    }
  if (errno)
    {
      TRACE ("Error: could not read \"/etc/firejail\" entry (%s)", strerror (errno));
    }

  g_dir_close (dir);

  /* we did not know the reading order in the directory */
  profiles = g_list_sort (profiles, (GCompareFunc) g_strcmp0);
  for (lp = profiles; lp != NULL; lp = lp->next)
    {
      gtk_combo_box_text_append_text (box, lp->data);
      g_free (lp->data);
    }
  g_list_free (profiles);

  gtk_combo_box_text_prepend_text (box, EXECHELP_DEFAULT_ROOT_PROFILE);
  gtk_combo_box_text_prepend_text (box, EXECHELP_DEFAULT_USER_PROFILE);

  return;
}

static void
thunar_protected_dialog_add_line (GtkButton *button,
                                  gpointer   user_data)
{
  ProtectionDialogData *data = user_data;
  GtkWidget      *w1, *w2;
  guint           row;

  /* create the new widget line */
  w1 = thunar_protected_chooser_button_new ();
  thunar_protected_chooser_button_update_list (THUNAR_PROTECTED_CHOOSER_BUTTON (w1), data->folders_only? NULL : data->apps, data->files->data);
  data->handlers = g_list_append (data->handlers, w1);

  w2 = gtk_combo_box_text_new ();
  thunar_protected_manager_populate_profile_box (w2);
  data->profiles = g_list_append (data->profiles, w2);

  g_signal_connect (w1, "changed", G_CALLBACK (thunar_protected_dialog_handler_changed_cb), w2);
  thunar_protected_dialog_handler_changed_cb (GTK_COMBO_BOX (w1), w2);

  /* pack the new line */
  gtk_table_get_size (data->content_area, &row, NULL);
  gtk_table_resize (data->content_area, row+1, 2);
  gtk_table_attach (data->content_area, w1, 0, 1, row, row+1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach (data->content_area, w2, 1, 2, row, row+1, GTK_FILL, GTK_FILL, 0, 0);

  gtk_widget_show_all (GTK_WIDGET (data->content_area));
}

ProtectionDialogData *
thunar_protected_show_protection_dialog (GtkWidget *parent, GList *files, gboolean policy_authoring_mode)
{
  ThunarProtectedManager *manager = thunar_protected_manager_get ();
  ThunarApplication      *application;
  ProtectionDialogData   *data = NULL;
  GtkWidget              *dialog, *widget, *box, *content_area, *table;
  GList                  *lp;

  g_return_val_if_fail (manager != NULL, NULL);
  g_return_val_if_fail (files != NULL, NULL);

  // create the dialog
  dialog = gtk_dialog_new_with_buttons (policy_authoring_mode? "Add Protected Files" : "Open Files in Sandbox",
                                        GTK_WINDOW (parent),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OK,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  box = gtk_vbox_new (FALSE, 12);
  gtk_container_add (GTK_CONTAINER (content_area), box);
  content_area = box;

  data = g_malloc0 (sizeof (ProtectionDialogData));
  data->dialog = dialog;
  data->user_data = NULL;
  data->files = g_list_copy_deep (files, (GCopyFunc) g_object_ref, NULL);
  data->apps = thunar_file_list_get_applications (files);

  // check if we only have folders or we also have files
  data->folders_only = TRUE;
  for (lp = data->files; lp != NULL && data->folders_only; lp = lp->next)
      data->folders_only = thunar_file_is_directory (lp->data);

  // label at the top of the dialog
  widget = gtk_label_new ("Select the applications that will be used to open the protected files, and the sandbox profiles applied to them. You can choose ‘Any Application’ if you want to enforce a sandbox profile regardless of the application.");
  gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);
  box = gtk_hbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 12);
  gtk_box_pack_start (GTK_BOX (content_area), box, FALSE, FALSE, 12);

  // header labels
  table = gtk_table_new (1, 2, TRUE);
  data->content_area = GTK_TABLE (table);
  gtk_box_pack_start (GTK_BOX (content_area), table, TRUE, TRUE, 12);

  widget = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (widget), "<b>Application to Use</b>");
  gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
  gtk_table_attach (GTK_TABLE (table), widget, 0, 1, 0, 1, GTK_EXPAND, GTK_FILL, 0, 0);

  widget = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (widget), "<b>Sandbox Profile</b>");
  gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
  gtk_table_attach (GTK_TABLE (table), widget, 1, 2, 0, 1, GTK_EXPAND, GTK_FILL, 0, 0);

  // add a first app / profile combo box line and a button to add another line
  thunar_protected_dialog_add_line (NULL, data);
  
  // add the "Add" button
  if (policy_authoring_mode)
  {
    widget = gtk_button_new_with_label ("Add an Application");
    box = gtk_hbox_new (FALSE, 12);
    gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 12);
    gtk_box_pack_start (GTK_BOX (content_area), box, FALSE, FALSE, 12);

    g_signal_connect (widget, "clicked", G_CALLBACK (thunar_protected_dialog_add_line), data);

    // connect all responses from dialog to the handler which will free the data
    g_signal_connect (dialog, "response", G_CALLBACK (thunar_protected_show_protection_dialog_cb), data);
  }

  // show the dialog
  gtk_widget_show_all (dialog);
  application = thunar_application_get ();
  thunar_application_take_window (application, GTK_WINDOW (dialog));
  g_object_unref (G_OBJECT (application));

  return data;
}

gboolean
thunar_protected_add_protected_file (ThunarFile *file, gchar *profiles)
{
  ThunarProtectedManager       *manager  = thunar_protected_manager_get ();
  ExecHelpProtectedFileHandler *h        = NULL;
  GList                        *list     = NULL;
  GList                        *item     = NULL;
  gchar                        *endptr   = NULL;
  gchar                        *path     = NULL;
  gchar                        *tmp      = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (profiles != NULL, FALSE);

  path = g_file_get_path (thunar_file_get_file (file));

  /* remove existing policy */
  if (g_hash_table_contains (manager->protected_files, path))
    {
      item = g_list_find_custom (manager->file_order, path, (GCompareFunc) g_strcmp0);
      if (item)
        manager->file_order = g_list_delete_link (manager->file_order, item);
      g_hash_table_remove (manager->protected_files, path);
    }

  /* add the new one */
  tmp = g_strdup (profiles);
  do {
    if (protected_files_parse_handler(tmp, &endptr, &h) == 0)
      list = g_list_prepend(list, h);

  } while (endptr);
  g_free (tmp);

  manager->file_order = g_list_append (manager->file_order, path);
  g_hash_table_insert (manager->protected_files, path, list);
  
  /* let the world know the file has changed */
  thunar_file_changed (file);
  g_signal_emit (manager, manager_signals[LIST_UPDATED], 0);

  manager->policy_dirty = TRUE;
  return TRUE;
}

gboolean
thunar_protected_remove_protected_file (ThunarFile *file)
{
  ThunarProtectedManager *manager = thunar_protected_manager_get ();
  gchar                  *path    = NULL;
  GList                  *item    = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  path = g_file_get_path (thunar_file_get_file (file));

  /* remove existing policy */
  if (g_hash_table_contains (manager->protected_files, path))
    {
      item = g_list_find_custom (manager->file_order, path, (GCompareFunc) g_strcmp0);
      if (item)
        manager->file_order = g_list_delete_link (manager->file_order, item);
      g_hash_table_remove (manager->protected_files, path);
    }

  g_free (path);

  /* let the world know the file has changed */
  thunar_file_changed (file);
  g_signal_emit (manager, manager_signals[LIST_UPDATED], 0);

  manager->policy_dirty = TRUE;
  return TRUE;
}

GList *
thunar_protected_get_applications_for_files (GList *files)
{
  ExecHelpProtectedFileHandler *merged;
  ExecHelpHandlerMergeResult    result;
  ExecHelpList                 *list;
  ExecHelpList                 *ehp;
  GList                        *applications = NULL;
  GList                        *prepend      = NULL;
  GList                        *fp;
  GList                        *ip;
  gchar                        *path;

  for (fp = files; fp != NULL; fp = fp->next)
    {
      path = g_file_get_path (thunar_file_get_file (fp->data));
      list = protected_files_get_handlers_for_file (path);
      TRACE ("ExecHelper: %s gives us %d items", path, exechelp_list_length (list));
      g_free (path);

      if (G_UNLIKELY (applications == NULL))
        {
          /* first file, so just use the applications list */
          for (ehp = list; ehp != NULL; ehp = ehp->next)
            applications = g_list_append (applications, ehp->data);
        }
      else
        {
          /* keep only the applications that are also present in list */
          for (ehp = list; ehp != NULL; ehp = ehp->next)
            {
              for (ip = applications; ip; ip = ip->next)
                {
                  merged = NULL;
                  result = protected_files_handlers_merge (ehp->data, ip->data, &merged);

                  if (result == HANDLER_IDENTICAL)
                    prepend = g_list_prepend (prepend, protected_files_handler_copy (ip->data, NULL));
                  else if (result == HANDLER_USE_MERGED)
                    prepend = g_list_prepend (prepend, merged);
                }

              g_list_free_full(applications, (ExecHelpDestroyNotify) protected_files_handler_free);
              applications = prepend;
            }
        }
    }

  TRACE ("ExecHelper: returning %d merged items", g_list_length (applications));
  return applications;
}



static gboolean
thunar_protected_manager_save_policy (ThunarProtectedManager *manager)
{
  FILE                         *fp       = NULL;
  char                         *path     = NULL;
  ExecHelpProtectedFileHandler *h        = NULL;
  GList                        *list     = NULL;
  GList                        *lp, *li;
  gint                          index;

  g_return_val_if_fail (manager != NULL, FALSE);

  if (protected_files_save_start (&fp) == -1)
    {
      fprintf (stderr, "ExecHelper: could not open a file handle to save the policy\n");
      return FALSE;
    }

  for (li = manager->file_order; li != NULL; li = li->next)
    {
      path = li->data;
      list = g_hash_table_lookup (manager->protected_files, path);

      if (!list)
        continue;

      if (protected_files_save_add_file_start (&fp, path) == -1)
        {
          fprintf (stderr, "ExecHelper: could not save file '%s'\n", path);
          continue;
        }

      for (index = 0, lp = list; lp != NULL; lp = lp->next)
        {
          h = lp ->data;
          if (protected_files_save_add_file_add_handler (&fp, h->handler_path, h->profile_name, index++) == -1)
            {
              fprintf (stderr, "ExecHelper: could not write profile to file '%s'\n", path);
              continue;
            }
        }

      if (protected_files_save_add_file_finish (&fp) == -1)
        {
          fprintf (stderr, "ExecHelper: could not save file '%s'\n", path);
          continue;
        }
    }

  if (protected_files_save_finish (&fp) == -1)
    {
      fprintf (stderr, "ExecHelper: could not properly close the file handle to which the policy was saved\n");
      return FALSE;
    }

  return TRUE;
}

gboolean
thunar_protected_manager_flush (void)
{
  ThunarProtectedManager *manager = thunar_protected_manager_get ();
  gboolean                success = TRUE;

  if (manager->policy_dirty)
    {
      success = thunar_protected_manager_save_policy (manager);

    if (success)
      manager->policy_dirty = FALSE;
    }

  return success;
}

static void
thunar_protected_manager_clear_policy (ThunarProtectedManager *manager)
{
  g_return_if_fail (manager != NULL);

  g_list_free (manager->file_order);
  manager->file_order = NULL;
  g_hash_table_remove_all (manager->protected_files);
  manager->policy_loaded = FALSE;
  manager->policy_dirty = TRUE;
}

gboolean
thunar_protected_manager_is_file_protected (ThunarFile *file)
{
  g_return_val_if_fail (file != NULL, FALSE);
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  return thunar_protected_manager_is_g_file_protected (thunar_file_get_file (file));
}

gboolean
thunar_protected_manager_is_file_protected_directly (ThunarFile *file)
{
  g_return_val_if_fail (file != NULL, FALSE);
  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  return thunar_protected_manager_is_g_file_protected_directly (thunar_file_get_file (file));
}

gboolean
thunar_protected_manager_is_g_file_protected (GFile *file)
{
  ThunarProtectedManager *manager = thunar_protected_manager_get ();
  gchar     *path      = NULL;
  gchar     *iter      = NULL;
  gboolean   protected = FALSE;

  g_return_val_if_fail (manager != NULL, FALSE);
  g_return_val_if_fail (file != NULL, FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  iter = path = g_file_get_path (file);
  while (iter && *iter && !protected)
  {
    protected = g_hash_table_contains (manager->protected_files, iter);

    iter = strrchr(iter, '/');
    if (iter)
    {
      *iter = '\0';
      iter = path;
    }
  }

  g_free (path);
  return protected;
}

gboolean
thunar_protected_manager_is_g_file_protected_directly (GFile *file)
{
  ThunarProtectedManager *manager = thunar_protected_manager_get ();
  gchar     *path      = NULL;
  gboolean   protected = FALSE;

  g_return_val_if_fail (manager != NULL, FALSE);
  g_return_val_if_fail (file != NULL, FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  path = g_file_get_path (file);
  if (path)
    protected = g_hash_table_contains (manager->protected_files, path);
  g_free (path);

  return protected;
}

static gboolean
thunar_protected_manager_load_policy (ThunarProtectedManager *manager)
{
  FILE                         *fp       = NULL;
  int                           lineno   = 0;
  char                         *path     = NULL;
  char                         *profiles = NULL;
  ExecHelpProtectedFileHandler *h        = NULL;
  char                         *endptr   = NULL;
  GList                        *list     = NULL;

  g_return_val_if_fail (manager != NULL, FALSE);

  if (manager->policy_loaded)
  {
    thunar_protected_manager_clear_policy (manager);
  }

  do
  {
    if (protected_files_parse (&fp, &lineno, &path, &profiles) == 0 && fp)
    {
      if (path && profiles)
      {
        TRACE ("ExecHelper: File %s is protected using the following policy:", path, profiles);

        h = NULL;
        endptr = NULL;
        list = NULL;
        do {
          if (protected_files_parse_handler(profiles, &endptr, &h) == 0) {
            if (!h)
            {
              fprintf (stderr, "ExecHelper: Unknown error occurred while parsing handlers in line %d of protected files policy\n", lineno);
            }
            else
            {
              TRACE ("ExecHelper: \t%s using profile '%s'\n", h->handler_path, h->profile_name);

              // prepending makes the policy items further down the file more prominent
              list = g_list_prepend(list, h);
            }
          }
        } while (endptr);

        manager->file_order = g_list_append (manager->file_order, path);
        g_hash_table_insert (manager->protected_files, path, list);
        free(profiles);
      }
    }
  } while (fp);

  manager->policy_loaded = TRUE;
  manager->policy_dirty = FALSE;
  g_signal_emit (manager, manager_signals[LIST_UPDATED], 0);

  return manager->policy_loaded;
}

static void
policy_list_item_free (gpointer object)
{
  ExecHelpProtectedFileHandler *hand = (ExecHelpProtectedFileHandler *) object;
  protected_files_handler_free (hand);
}

static void
policy_list_free (gpointer object)
{
  GList *list = (GList *) object;
  g_list_free_full (list, policy_list_item_free);
}

static void
thunar_protected_manager_init (ThunarProtectedManager *manager)
{
  g_return_if_fail (manager != NULL);

  manager->policy_loaded   = FALSE;
  manager->policy_dirty    = FALSE;
  manager->file_order      = NULL;
  manager->protected_files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, policy_list_free);

  thunar_protected_manager_load_policy (manager);
}


static void
thunar_protected_manager_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (thunar_protected_manager_parent_class)->finalize) (object);
}


/**
 * thunar_protected_manager_new:
 *
 * Allocates a new #ThunarProtectedManager instance.
 *
 * Return value: the newly allocated #ThunarProtectedManager instance.
 **/
ThunarProtectedManager*
thunar_protected_manager_new (void)
{
  return g_object_new (THUNAR_TYPE_PROTECTED_MANAGER, NULL);
}


/**
 * thunar_protected_manager_get:
 *
 * Retrieves a global #ThunarProtectedManager instance, allocatng it if
 * necessary.
 *
 * Return value: the global #ThunarProtectedManager instance.
 **/
ThunarProtectedManager*
thunar_protected_manager_get (void)
{
  static ThunarProtectedManager *inst = NULL;
  if (!inst)
    inst = thunar_protected_manager_new ();

  return inst;
}
