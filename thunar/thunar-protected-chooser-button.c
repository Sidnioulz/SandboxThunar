/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2010      Nick Schermer <nick@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
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

#include <thunar/thunar-protected-chooser-button.h>
#include <thunar/thunar-protected-chooser-dialog.h>
#include <thunar/thunar-protected-chooser-model.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-private.h>
#include <gtk/gtk.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_INFO,
};



static void     thunar_protected_chooser_button_finalize          (GObject             *object);
static void     thunar_protected_chooser_button_get_property      (GObject             *object,
                                                                   guint                prop_id,
                                                                   GValue              *value,
                                                                   GParamSpec          *pspec);
static void     thunar_protected_chooser_button_set_property      (GObject             *object,
                                                                   guint                prop_id,
                                                                   const GValue        *value,
                                                                   GParamSpec          *pspec);
static gboolean thunar_protected_chooser_button_scroll_event      (GtkWidget           *widget,
                                                                   GdkEventScroll      *event);
static void     thunar_protected_chooser_button_set_current_info  (ThunarProtectedChooserButton *chooser_button,
                                                                   GAppInfo                     *app_info);
static void     thunar_protected_chooser_button_changed           (GtkComboBox         *combo_box);
static gint     thunar_protected_chooser_button_sort_applications (gconstpointer                 a,
                                                                   gconstpointer                 b);
static gboolean thunar_protected_chooser_button_row_separator     (GtkTreeModel                 *model,
                                                                   GtkTreeIter                  *iter,
                                                                   gpointer                      data);
static void     thunar_protected_chooser_button_chooser_dialog    (ThunarProtectedChooserButton *chooser_button);



struct _ThunarProtectedChooserButtonClass
{
  GtkComboBoxClass __parent__;
};

struct _ThunarProtectedChooserButton
{
  GtkComboBox   __parent__;

  GtkListStore *store;
  GAppInfo     *current_info;
};



G_DEFINE_TYPE (ThunarProtectedChooserButton, thunar_protected_chooser_button, GTK_TYPE_COMBO_BOX)



static void
thunar_protected_chooser_button_class_init (ThunarProtectedChooserButtonClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_protected_chooser_button_finalize;
  gobject_class->get_property = thunar_protected_chooser_button_get_property;
  gobject_class->set_property = thunar_protected_chooser_button_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->scroll_event = thunar_protected_chooser_button_scroll_event;

  /**
   * ThunarProtectedChooserButton:current_info:
   *
   * A placeholder for the #GAppInfo currently chosen.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CURRENT_INFO,
                                   g_param_spec_object ("current-info", "current-info", "current-info",
                                                        G_TYPE_APP_INFO,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_protected_chooser_button_init (ThunarProtectedChooserButton *chooser_button)
{
  GtkCellRenderer *renderer;

  /* start with nothing */
  chooser_button->current_info = NULL;

  /* allocate a new store for the combo box */
  chooser_button->store = gtk_list_store_new (THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_N_COLUMNS,
                                              G_TYPE_STRING,
                                              G_TYPE_ICON,
                                              G_TYPE_OBJECT,
                                              G_TYPE_BOOLEAN,
                                              PANGO_TYPE_STYLE);
  gtk_combo_box_set_model (GTK_COMBO_BOX (chooser_button), 
                           GTK_TREE_MODEL (chooser_button->store));

  g_signal_connect (chooser_button, "changed", 
                    G_CALLBACK (thunar_protected_chooser_button_changed), NULL);

  /* set separator function */
  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (chooser_button),
                                        thunar_protected_chooser_button_row_separator,
                                        NULL, NULL);

  /* add renderer for the application icon */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (chooser_button), renderer, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (chooser_button), renderer,
                                  "gicon", 
                                  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_ICON,
                                  "sensitive", 
                                  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_SENSITIVE,
                                  NULL);

  /* add renderer for the application name */
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (chooser_button), renderer, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (chooser_button), renderer,
                                  "text", 
                                  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_NAME,
                                  "sensitive", 
                                  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_SENSITIVE,
                                  "style",
                                  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_STYLE,
                                  NULL);
}



static void
thunar_protected_chooser_button_finalize (GObject *object)
{
  ThunarProtectedChooserButton *chooser_button = THUNAR_PROTECTED_CHOOSER_BUTTON (object);

  /* release the store */
  g_object_unref (G_OBJECT (chooser_button->store));

  if (chooser_button->current_info)
  {
    g_object_unref (chooser_button->current_info);
    chooser_button->current_info = NULL;
  }

  (*G_OBJECT_CLASS (thunar_protected_chooser_button_parent_class)->finalize) (object);
}



static void
thunar_protected_chooser_button_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ThunarProtectedChooserButton *chooser_button = THUNAR_PROTECTED_CHOOSER_BUTTON (object);

  switch (prop_id)
    {
    case PROP_CURRENT_INFO:
      g_value_set_object (value, chooser_button->current_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_protected_chooser_button_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ThunarProtectedChooserButton *chooser_button = THUNAR_PROTECTED_CHOOSER_BUTTON (object);

  switch (prop_id)
    {
    case PROP_CURRENT_INFO:
      thunar_protected_chooser_button_set_current_info (chooser_button, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_protected_chooser_button_scroll_event (GtkWidget      *widget,
                                    GdkEventScroll *event)
{
  ThunarProtectedChooserButton *chooser_button = THUNAR_PROTECTED_CHOOSER_BUTTON (widget);
  GtkTreeIter          iter;
  GObject             *application;
  GtkTreeModel        *model = GTK_TREE_MODEL (chooser_button->store);

  g_return_val_if_fail (THUNAR_IS_PROTECTED_CHOOSER_BUTTON (chooser_button), FALSE);

  /* check if the next application in the store is valid if we scroll down,
   * else drop the event so we don't popup the chooser dailog */
  if (event->direction != GDK_SCROLL_UP
      && gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter)
      && gtk_tree_model_iter_next (model, &iter))
    {
      gtk_tree_model_get (model, &iter, 
                          THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_APPLICATION, 
                          &application, -1);

      if (application == NULL)
        return FALSE;

      g_object_unref (G_OBJECT (application));
    }

  return (*GTK_WIDGET_CLASS (thunar_protected_chooser_button_parent_class)->scroll_event) (widget, event);
}



static void
thunar_protected_chooser_button_set_current_info (ThunarProtectedChooserButton *chooser_button, GAppInfo *app_info)
{
  GAppInfo *old_info = chooser_button->current_info;
  chooser_button->current_info = app_info ? g_object_ref (app_info) : NULL;
  if (old_info)
    {
      g_object_unref (old_info);
    }

  /*if (app_info == NULL)
    {
      g_signal_handlers_block_by_func (G_OBJECT (chooser_button), thunar_protected_chooser_button_changed, NULL);
      gtk_combo_box_set_active (GTK_COMBO_BOX (chooser_button), -1);
      g_signal_handlers_unblock_by_func (G_OBJECT (chooser_button), thunar_protected_chooser_button_changed, NULL);
    }*/
}



static void
thunar_protected_chooser_button_changed (GtkComboBox *combo_box)
{
  ThunarProtectedChooserButton *chooser_button = THUNAR_PROTECTED_CHOOSER_BUTTON (combo_box);
  GtkTreeIter          iter;
  GAppInfo            *app_info;

  _thunar_return_if_fail (THUNAR_IS_PROTECTED_CHOOSER_BUTTON (chooser_button));
  _thunar_return_if_fail (GTK_IS_LIST_STORE (chooser_button->store));

  /* get the selected item in the combo box */
  if (!gtk_combo_box_get_active_iter (combo_box, &iter))
    return;

  /* determine the application that was set for the item */
  gtk_tree_model_get (GTK_TREE_MODEL (chooser_button->store), &iter,
                      THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_APPLICATION,
                      &app_info, -1);

  if (G_LIKELY (app_info != NULL))
    {
      thunar_protected_chooser_button_set_current_info (chooser_button, app_info);
      g_object_unref (app_info);
    }
  else
    {
      /* no application was found in the store, looks like the other... option */
      thunar_protected_chooser_button_chooser_dialog (chooser_button);
    }
}



static gint
thunar_protected_chooser_button_sort_applications (gconstpointer a,
                                         gconstpointer b)
{
  _thunar_return_val_if_fail (G_IS_APP_INFO (a), -1);
  _thunar_return_val_if_fail (G_IS_APP_INFO (b), -1);

  return g_utf8_collate (g_app_info_get_name (G_APP_INFO (a)),
                         g_app_info_get_name (G_APP_INFO (b)));
}



static gboolean
thunar_protected_chooser_button_row_separator (GtkTreeModel *model,
                                     GtkTreeIter  *iter,
                                     gpointer      data)
{
  gchar *name;

  /* determine the value of the "name" column */
  gtk_tree_model_get (model, iter, THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_NAME, &name, -1);
  if (G_LIKELY (name != NULL))
    {
      g_free (name);
      return FALSE;
    }

  return TRUE;
}



static void
thunar_protected_chooser_button_chooser_dialog (ThunarProtectedChooserButton *chooser_button)
{
  GtkWidget    *toplevel;
  ThunarProtectedChooserModel *model;
  GtkTreeIter   iter;
  GtkWidget    *dialog;
  GAppInfo     *info, *iter_app;
  gboolean      found = FALSE;
  int           response;

  _thunar_return_if_fail (THUNAR_IS_PROTECTED_CHOOSER_BUTTON (chooser_button));

  /* determine the toplevel window for the chooser */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (chooser_button));
  if (G_UNLIKELY (toplevel == NULL))
    return;

  /* popup the application chooser dialog */
  dialog = g_object_new (THUNAR_TYPE_PROTECTED_CHOOSER_DIALOG, NULL);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  /* fill up the dialog */
  model = thunar_protected_chooser_model_new ("*");
  thunar_protected_chooser_dialog_set_model (THUNAR_PROTECTED_CHOOSER_DIALOG (dialog), model);
  g_object_unref (G_OBJECT (model));

  /* process the response, possibly add an app to the button list */
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT)
    {
      info = thunar_protected_chooser_dialog_get_current_info (THUNAR_PROTECTED_CHOOSER_DIALOG (dialog));
      if (info)
        {
          for (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (chooser_button->store), &iter);
               gtk_tree_model_iter_next (GTK_TREE_MODEL (chooser_button->store), &iter) && !found;)
            {
              gtk_tree_model_get (GTK_TREE_MODEL (chooser_button->store), &iter,
                                  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_APPLICATION,
                                  &iter_app, -1);
              if (iter_app)
                {
                  if (g_app_info_equal (info, iter_app))
                  {
                    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (chooser_button), &iter);
                    found = TRUE;
                  }

                  g_object_unref (iter_app);
                }
            }
          if (!found)
            {
              gint n = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (chooser_button->store), NULL);
              gtk_list_store_insert (chooser_button->store, &iter,
                                      n-2);
              gtk_list_store_set (chooser_button->store, &iter,
                                  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_NAME,
                                  g_app_info_get_name (info),
                                  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_APPLICATION,
                                  info,
                                  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_ICON,
                                  g_app_info_get_icon (info),
                                  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_SENSITIVE,
                                  TRUE,
                                  -1);
              gtk_combo_box_set_active_iter (GTK_COMBO_BOX (chooser_button), &iter);
            }
          else
            {
              g_object_unref (info);
            }
        }
    }
  
  gtk_widget_destroy (dialog);
}



void
thunar_protected_chooser_button_update_list (ThunarProtectedChooserButton *chooser_button,
                                             GList                        *apps,
                                             ThunarFile                   *file)
{
  GtkTreeIter  iter;
  gboolean     set_active = FALSE;
  GAppInfo    *default_info = NULL;
  GAppInfo    *exo_app, *current;
  GError      *error = NULL;
  GList       *app_infos;
  GList       *lp;
  guint        i = 0;

  _thunar_return_if_fail (THUNAR_IS_PROTECTED_CHOOSER_BUTTON (chooser_button));
  
  /* clear the store */
  gtk_list_store_clear (chooser_button->store);

  /* block the changed signal for a moment */
  g_signal_handlers_block_by_func (chooser_button,
                                   thunar_protected_chooser_button_changed,
                                   NULL);

  /* setup a useful tooltip for the button */
  thunar_gtk_widget_set_tooltip (GTK_WIDGET (chooser_button),
                                 _("The selected application will be used to "
                                   "open protected files."));

  if (apps)
    {
      app_infos = g_list_sort (g_list_copy (apps), thunar_protected_chooser_button_sort_applications);
    
      if (file)
        {
          _thunar_return_if_fail (THUNAR_IS_FILE (file));

          /* determine the default application for the file */
          default_info = thunar_file_get_default_handler (file);
        }

      /* add all possible applications */
      for (lp = app_infos, i = 0; lp != NULL; lp = lp->next, ++i)
        {
          if (thunar_g_app_info_should_show (lp->data))
            {
              /* insert the item into the store */
              gtk_list_store_insert_with_values (chooser_button->store, &iter, i,
                                                 THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_NAME,
                                                 g_app_info_get_name (lp->data),
                                                 THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_APPLICATION,
                                                 g_object_ref (lp->data),
                                                 THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_ICON,
                                                 g_app_info_get_icon (lp->data),
                                                 THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_SENSITIVE,
                                                 TRUE,
                                                 -1);

              /* pre-select the default application */
              if (default_info != NULL && !set_active && g_app_info_equal (lp->data, default_info))
                {
                  gtk_combo_box_set_active_iter (GTK_COMBO_BOX (chooser_button), &iter);
                  set_active = TRUE;
                }
            }

          /* release the application */
          g_object_unref (lp->data);
        }

      /* release the default application */
      if (default_info)
          g_object_unref (default_info);
      g_list_free (app_infos);

      /* add the "Other Application..." option */
      gtk_list_store_insert_with_values (chooser_button->store, &iter, ++i,
                                         THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_NAME,
                                         _("Other Application..."),
                                         THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_SENSITIVE,
                                         TRUE,
                                         -1);
    }
  else
    {
      /* determine all applications */
      app_infos = g_app_info_get_all ();
      app_infos = g_list_sort (app_infos, thunar_protected_chooser_button_sort_applications);
      
      /* add all possible applications */
      for (lp = app_infos, i = 0; lp != NULL; lp = lp->next, ++i)
        {
          if (thunar_g_app_info_should_show (lp->data))
            {
              /* insert the item into the store */
              gtk_list_store_insert_with_values (chooser_button->store, &iter, i,
                                                 THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_NAME,
                                                 g_app_info_get_name (lp->data),
                                                 THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_APPLICATION,
                                                 g_object_ref (lp->data),
                                                 THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_ICON,
                                                 g_app_info_get_icon (lp->data),
                                                 THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_SENSITIVE,
                                                 TRUE,
                                                 -1);
              
            }

          /* release the application */
          g_object_unref (lp->data);
        }

      /* release the application list */
      g_list_free (app_infos);
    }

  /* insert empty row that will appear as a separator */
  gtk_list_store_insert_with_values (chooser_button->store, NULL, ++i, -1);

  /* add the "Any Application..." option */
  exo_app = g_app_info_create_from_commandline ("exo-open %F",
                                    _("Any Application..."),
                                    G_APP_INFO_CREATE_SUPPORTS_URIS,
                                    &error);
  if (error)
    {
      g_error_free (error);
    }
  else
    {
      gtk_list_store_insert_with_values (chooser_button->store, NULL, 0, -1);
      gtk_list_store_insert_with_values (chooser_button->store, &iter, 0,
                                         THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_NAME,
                                         _("Any Application..."),
                                         THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_APPLICATION,
                                         exo_app,
                                         THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_SENSITIVE,
                                         TRUE,
                                         -1);
      if (!set_active)
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (chooser_button), &iter);
    }

  /* update current_info */
//TODO
  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (chooser_button), &iter))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (chooser_button->store), &iter,
                          THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_APPLICATION, 
                          &current, -1);
    }

  thunar_protected_chooser_button_set_current_info (chooser_button, current);
  if (current)
    g_object_unref (current);

  /* unblock the changed signal */
  g_signal_handlers_unblock_by_func (chooser_button,
                                     thunar_protected_chooser_button_changed,
                                     NULL);
}



/**
 * thunar_protected_chooser_button_new:
 *
 * Allocates a new #ThunarProtectedChooserButton instance.
 *
 * Return value: the newly allocated #ThunarProtectedChooserButton.
 **/
GtkWidget*
thunar_protected_chooser_button_new (void)
{
  return g_object_new (THUNAR_TYPE_PROTECTED_CHOOSER_BUTTON, NULL);
}



/**
 * thunar_protected_chooser_button_get_current_info:
 * @chooser_button : a #ThunarProtectedChooserButton instance.
 *
 * Returns the #GAppInfo current chosen, if any, or NULL otherwise.
 **/
GAppInfo *
thunar_protected_chooser_button_get_current_info (ThunarProtectedChooserButton *chooser_button)
{
  _thunar_return_val_if_fail (THUNAR_IS_PROTECTED_CHOOSER_BUTTON (chooser_button), NULL);

  if (chooser_button->current_info)
      return g_object_ref (chooser_button->current_info);
  else
      return NULL;
}



