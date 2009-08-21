/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>

#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-io-scan-directory.h>
#include <thunar/thunar-job.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-simple-job.h>
#include <thunar/thunar-transfer-job.h>



static GList *
_tij_collect_nofollow (ThunarJob *job,
                       GList     *base_file_list,
                       GError   **error)
{
  GError *err = NULL;
  GList  *child_file_list = NULL;
  GList  *file_list = NULL;
  GList  *lp;

  /* recursively collect the files */
  for (lp = base_file_list; 
       err == NULL && lp != NULL && !exo_job_is_cancelled (EXO_JOB (job)); 
       lp = lp->next)
    {
      /* try to scan the directory */
      child_file_list = thunar_io_scan_directory (job, lp->data, 
                                                  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, 
                                                  TRUE, &err);

      /* prepend the new files to the existing list */
      file_list = thunar_g_file_list_prepend (file_list, lp->data);
      file_list = g_list_concat (child_file_list, file_list);
    }

  /* check if we failed */
  if (err != NULL || exo_job_is_cancelled (EXO_JOB (job)))
    {
      if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
        g_error_free (err);
      else
        g_propagate_error (error, err);

      /* release the collected files */
      thunar_g_file_list_free (file_list);

      return NULL;
    }

  return file_list;
}



static gboolean
_thunar_io_jobs_create (ThunarJob   *job,
                        GValueArray *param_values,
                        GError     **error)
{
  GFileOutputStream *stream;
  ThunarJobResponse  response = THUNAR_JOB_RESPONSE_CANCEL;
  GFileInfo         *info;
  GError            *err = NULL;
  GList             *file_list;
  GList             *lp;
  gchar             *basename;
  gchar             *display_name;
  
  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (param_values != NULL, FALSE);
  _thunar_return_val_if_fail (param_values->n_values == 1, FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* get the file list */
  file_list = g_value_get_boxed (g_value_array_get_nth (param_values, 0));

  /* we know the total amount of files to be processed */
  thunar_job_set_total_files (THUNAR_JOB (job), file_list);

  /* iterate over all files in the list */
  for (lp = file_list; 
       err == NULL && lp != NULL && !exo_job_is_cancelled (EXO_JOB (job)); 
       lp = lp->next)
    {
      g_assert (G_IS_FILE (lp->data));

      /* update progress information */
      thunar_job_processing_file (THUNAR_JOB (job), lp);

again:
      /* try to create the file */
      stream = g_file_create (lp->data, 
                              G_FILE_CREATE_NONE, 
                              exo_job_get_cancellable (EXO_JOB (job)),
                              &err);

      /* abort if the job was cancelled */
      if (exo_job_is_cancelled (EXO_JOB (job)))
        break;

      /* check if creating failed */
      if (stream == NULL)
        {
          if (err->code == G_IO_ERROR_EXISTS)
            {
              g_clear_error (&err);

              /* the file already exists, query its display name */
              info = g_file_query_info (lp->data,
                                        G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                        G_FILE_QUERY_INFO_NONE,
                                        exo_job_get_cancellable (EXO_JOB (job)),
                                        NULL);

              /* abort if the job was cancelled */
              if (exo_job_is_cancelled (EXO_JOB (job)))
                break;

              /* determine the display name, using the basename as a fallback */
              if (info != NULL)
                {
                  display_name = g_strdup (g_file_info_get_display_name (info));
                  g_object_unref (info);
                }
              else
                {
                  basename = g_file_get_basename (lp->data);
                  display_name = g_filename_display_name (basename);
                  g_free (basename);
                }

              /* ask the user whether he wants to overwrite the existing file */
              response = thunar_job_ask_overwrite (THUNAR_JOB (job), 
                                                   _("The file \"%s\" already exists"), 
                                                   display_name);

              /* check if we should overwrite */
              if (response == THUNAR_JOB_RESPONSE_YES)
                {
                  /* try to remove the file. fail if not possible */
                  if (g_file_delete (lp->data, exo_job_get_cancellable (EXO_JOB (job)), &err))
                    goto again;
                }
              
              /* clean up */
              g_free (display_name);
            }
          else 
            {
              /* determine display name of the file */
              basename = g_file_get_basename (lp->data);
              display_name = g_filename_display_basename (basename);
              g_free (basename);

              /* ask the user whether to skip/retry this path (cancels the job if not) */
              response = thunar_job_ask_skip (THUNAR_JOB (job), 
                                              _("Failed to create empty file \"%s\": %s"),
                                              display_name, err->message);
              g_free (display_name);

              g_clear_error (&err);

              /* go back to the beginning if the user wants to retry */
              if (response == THUNAR_JOB_RESPONSE_RETRY)
                goto again;
            }
        }
      else
        g_object_unref (stream);
    }

  /* check if we have failed */
  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  /* check if the job was cancelled */
  if (exo_job_is_cancelled (EXO_JOB (job)))
    return FALSE;

  /* emit the "new-files" signal with the given file list */
  thunar_job_new_files (THUNAR_JOB (job), file_list);

  return TRUE;
}



ThunarJob *
thunar_io_jobs_create_files (GList *file_list)
{
  return thunar_simple_job_launch (_thunar_io_jobs_create, 1,
                                   THUNAR_TYPE_G_FILE_LIST, file_list);
}



static gboolean
_thunar_io_jobs_mkdir (ThunarJob   *job,
                       GValueArray *param_values,
                       GError     **error)
{
  ThunarJobResponse response;
  GFileInfo        *info;
  GError           *err = NULL;
  GList            *file_list;
  GList            *lp;
  gchar            *basename;
  gchar            *display_name;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (param_values != NULL, FALSE);
  _thunar_return_val_if_fail (param_values->n_values == 1, FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  file_list = g_value_get_boxed (g_value_array_get_nth (param_values, 0));

  /* we know the total list of files to process */
  thunar_job_set_total_files (THUNAR_JOB (job), file_list);

  for (lp = file_list; 
       err == NULL && lp != NULL && !exo_job_is_cancelled (EXO_JOB (job));
       lp = lp->next)
    {
      g_assert (G_IS_FILE (lp->data));

      /* update progress information */
      thunar_job_processing_file (THUNAR_JOB (job), lp);

again:
      /* try to create the directory */
      if (!g_file_make_directory (lp->data, exo_job_get_cancellable (EXO_JOB (job)), &err))
        {
          if (err->code == G_IO_ERROR_EXISTS)
            {
              g_error_free (err);
              err = NULL;

              /* abort if the job was cancelled */
              if (exo_job_is_cancelled (EXO_JOB (job)))
                break;

              /* the file already exists, query its display name */
              info = g_file_query_info (lp->data,
                                        G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                        G_FILE_QUERY_INFO_NONE,
                                        exo_job_get_cancellable (EXO_JOB (job)),
                                        NULL);

              /* abort if the job was cancelled */
              if (exo_job_is_cancelled (EXO_JOB (job)))
                break;

              /* determine the display name, using the basename as a fallback */
              if (info != NULL)
                {
                  display_name = g_strdup (g_file_info_get_display_name (info));
                  g_object_unref (info);
                }
              else
                {
                  basename = g_file_get_basename (lp->data);
                  display_name = g_filename_display_name (basename);
                  g_free (basename);
                }

              /* ask the user whether he wants to overwrite the existing file */
              response = thunar_job_ask_overwrite (THUNAR_JOB (job), 
                                                   _("The file \"%s\" already exists"),
                                                   display_name);

              /* check if we should overwrite it */
              if (response == THUNAR_JOB_RESPONSE_YES)
                {
                  /* try to remove the file, fail if not possible */
                  if (g_file_delete (lp->data, exo_job_get_cancellable (EXO_JOB (job)), &err))
                    goto again;
                }

              /* clean up */
              g_free (display_name);
            }
          else
            {
              /* determine the display name of the file */
              basename = g_file_get_basename (lp->data);
              display_name = g_filename_display_basename (basename);
              g_free (basename);

              /* ask the user whether to skip/retry this path (cancels the job if not) */
              response = thunar_job_ask_skip (THUNAR_JOB (job), 
                                              _("Failed to create directory \"%s\": %s"),
                                              display_name, err->message);
              g_free (display_name);

              g_error_free (err);
              err = NULL;

              /* go back to the beginning if the user wants to retry */
              if (response == THUNAR_JOB_RESPONSE_RETRY)
                goto again;
            }
        }
    }

  /* check if we have failed */
  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  /* check if the job was cancelled */
  if (exo_job_is_cancelled (EXO_JOB (job)))
    return FALSE;

  /* emit the "new-files" signal with the given file list */
  thunar_job_new_files (THUNAR_JOB (job), file_list);
  
  return TRUE;
}



ThunarJob *
thunar_io_jobs_make_directories (GList *file_list)
{
  return thunar_simple_job_launch (_thunar_io_jobs_mkdir, 1,
                                   THUNAR_TYPE_G_FILE_LIST, file_list);
}



static gboolean
_thunar_io_jobs_unlink (ThunarJob   *job,
                        GValueArray *param_values,
                        GError     **error)
{
  ThunarJobResponse response;
  GFileInfo        *info;
  GError           *err = NULL;
  GList            *file_list;
  GList            *lp;
  gchar            *basename;
  gchar            *display_name;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (param_values != NULL, FALSE);
  _thunar_return_val_if_fail (param_values->n_values == 1, FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* get the file list */
  file_list = g_value_get_boxed (g_value_array_get_nth (param_values, 0));

  /* tell the user that we're preparing to unlink the files */
  exo_job_info_message (EXO_JOB (job), _("Preparing..."));

  /* recursively collect files for removal, not following any symlinks */
  file_list = _tij_collect_nofollow (job, file_list, &err);

  /* free the file list and fail if there was an error or the job was cancelled */
  if (err != NULL || exo_job_is_cancelled (EXO_JOB (job)))
    {
      if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
        g_error_free (err);
      else
        g_propagate_error (error, err);

      thunar_g_file_list_free (file_list);
      return FALSE;
    }

  /* we know the total list of files to process */
  thunar_job_set_total_files (THUNAR_JOB (job), file_list);

  /* remove all the files */
  for (lp = file_list; lp != NULL && !exo_job_is_cancelled (EXO_JOB (job)); lp = lp->next)
    {
      g_assert (G_IS_FILE (lp->data));

      /* skip root folders which cannot be deleted anyway */
      if (thunar_g_file_is_root (lp->data))
        continue;

again:
      /* try to delete the file */
      if (!g_file_delete (lp->data, exo_job_get_cancellable (EXO_JOB (job)), &err))
        {
          /* query the file info for the display name */
          info = g_file_query_info (lp->data, 
                                    G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                    G_FILE_QUERY_INFO_NONE, 
                                    exo_job_get_cancellable (EXO_JOB (job)), 
                                    NULL);

          /* abort if the job was cancelled */
          if (exo_job_is_cancelled (EXO_JOB (job)))
            {
              g_clear_error (&err);
              break;
            }

          /* determine the display name, using the basename as a fallback */
          if (info != NULL)
            {
              display_name = g_strdup (g_file_info_get_display_name (info));
              g_object_unref (info);
            }
          else
            {
              basename = g_file_get_basename (lp->data);
              display_name = g_filename_display_name (basename);
              g_free (basename);
            }

          /* ask the user whether he wants to skip this file */
          response = thunar_job_ask_skip (THUNAR_JOB (job), 
                                          _("Could not delete file \"%s\": %s"), 
                                          display_name, err->message);
          g_free (display_name);

          /* clear the error */
          g_clear_error (&err);

          /* check whether to retry */
          if (response == THUNAR_JOB_RESPONSE_RETRY)
            goto again;
        }
    }

  /* release the file list */
  thunar_g_file_list_free (file_list);

  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return FALSE;
  else
    return TRUE;
}



ThunarJob *
thunar_io_jobs_unlink_files (GList *file_list)
{
  return thunar_simple_job_launch (_thunar_io_jobs_unlink, 1,
                                   THUNAR_TYPE_G_FILE_LIST, file_list);
}



ThunarJob *
thunar_io_jobs_move_files (GList *source_file_list,
                           GList *target_file_list)
{
  ThunarJob *job;

  _thunar_return_val_if_fail (source_file_list != NULL, NULL);
  _thunar_return_val_if_fail (target_file_list != NULL, NULL);
  _thunar_return_val_if_fail (g_list_length (source_file_list) == g_list_length (target_file_list), NULL);

  job = thunar_transfer_job_new (source_file_list, target_file_list, 
                                 THUNAR_TRANSFER_JOB_MOVE);
  
  return THUNAR_JOB (exo_job_launch (EXO_JOB (job)));
}



ThunarJob *
thunar_io_jobs_copy_files (GList *source_file_list,
                           GList *target_file_list)
{
  ThunarJob *job;

  _thunar_return_val_if_fail (source_file_list != NULL, NULL);
  _thunar_return_val_if_fail (target_file_list != NULL, NULL);
  _thunar_return_val_if_fail (g_list_length (source_file_list) == g_list_length (target_file_list), NULL);

  job = thunar_transfer_job_new (source_file_list, target_file_list,
                                 THUNAR_TRANSFER_JOB_COPY);

  return THUNAR_JOB (exo_job_launch (EXO_JOB (job)));
}



static gboolean
_thunar_io_jobs_link (ThunarJob   *job,
                      GValueArray *param_values,
                      GError     **error)
{
  ThunarJobResponse response;
  GError           *err = NULL;
  GList            *new_files_list = NULL;
  GList            *source_file_list;
  GList            *sp;
  GList            *target_file_list;
  GList            *tp;
  gchar            *basename;
  gchar            *display_name;
  gchar            *source_path;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (param_values != NULL, FALSE);
  _thunar_return_val_if_fail (param_values->n_values == 2, FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  source_file_list = g_value_get_boxed (g_value_array_get_nth (param_values, 0));
  target_file_list = g_value_get_boxed (g_value_array_get_nth (param_values, 1));

  /* we know the total list of paths to process */
  thunar_job_set_total_files (THUNAR_JOB (job), source_file_list);

  /* process all files */
  for (sp = source_file_list, tp = target_file_list;
       err == NULL && sp != NULL && tp != NULL;
       sp = sp->next, tp = tp->next)
    {
      _thunar_assert (G_IS_FILE (sp->data));
      _thunar_assert (G_IS_FILE (tp->data));

      /* update progress information */
      thunar_job_processing_file (THUNAR_JOB (job), sp);

again:
      source_path = g_file_get_path (sp->data);

      if (G_LIKELY (source_path != NULL))
        {
          /* try to create the symlink */
          g_file_make_symbolic_link (tp->data, source_path, 
                                     exo_job_get_cancellable (EXO_JOB (job)),
                                     &err);

          g_free (source_path);

          if (err == NULL)
            new_files_list = thunar_g_file_list_prepend (new_files_list, sp->data);
          else
            {
              /* check if we have an error from which we can recover */
              if (err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
                {
                  /* ask the user whether he wants to overwrite the existing file */
                  response = thunar_job_ask_overwrite (THUNAR_JOB (job), "%s", 
                                                       err->message);

                  /* release the error */
                  g_clear_error (&err);

                  /* try to delete the file */
                  if (G_LIKELY (response == THUNAR_JOB_RESPONSE_YES))
                    {
                      /* try to remove the target file (fail if not possible) */
                      if (g_file_delete (tp->data, exo_job_get_cancellable (EXO_JOB (job)), &err))
                        goto again;
                    }
                }
            }
        }
      else
        {
          basename = g_file_get_basename (sp->data);
          display_name = g_filename_display_name (basename);
          g_set_error (&err, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       _("Could not create symbolic link to \"%s\" "
                         "because it is not a local file"), display_name);
          g_free (display_name);
          g_free (basename);
        }
    }

  if (err != NULL)
    {
      thunar_g_file_list_free (new_files_list);
      g_propagate_error (error, err);
      return FALSE;
    }
  else
    {
      thunar_job_new_files (THUNAR_JOB (job), new_files_list);
      thunar_g_file_list_free (new_files_list);
      return TRUE;
    }
}



ThunarJob *
thunar_io_jobs_link_files (GList *source_file_list,
                           GList *target_file_list)
{
  _thunar_return_val_if_fail (source_file_list != NULL, NULL);
  _thunar_return_val_if_fail (target_file_list != NULL, NULL);
  _thunar_return_val_if_fail (g_list_length (source_file_list) == g_list_length (target_file_list), NULL);

  return thunar_simple_job_launch (_thunar_io_jobs_link, 2,
                                   THUNAR_TYPE_G_FILE_LIST, source_file_list,
                                   THUNAR_TYPE_G_FILE_LIST, target_file_list);
}



static gboolean
_thunar_io_jobs_trash (ThunarJob   *job,
                       GValueArray *param_values,
                       GError     **error)
{
  GError *err = NULL;
  GList  *file_list;
  GList  *lp;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (param_values != NULL, FALSE);
  _thunar_return_val_if_fail (param_values->n_values == 1, FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  file_list = g_value_get_boxed (g_value_array_get_nth (param_values, 0));

  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return FALSE;

  for (lp = file_list; err == NULL && lp != NULL; lp = lp->next)
    {
      _thunar_assert (G_IS_FILE (lp->data));
      g_file_trash (lp->data, exo_job_get_cancellable (EXO_JOB (job)), &err);
    }

  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}



ThunarJob *
thunar_io_jobs_trash_files (GList *file_list)
{
  _thunar_return_val_if_fail (file_list != NULL, NULL);

  return thunar_simple_job_launch (_thunar_io_jobs_trash, 1,
                                   THUNAR_TYPE_G_FILE_LIST, file_list);
}



ThunarJob *
thunar_io_jobs_restore_files (GList *source_file_list,
                              GList *target_file_list)
{
  ThunarJob *job;

  _thunar_return_val_if_fail (source_file_list != NULL, NULL);
  _thunar_return_val_if_fail (target_file_list != NULL, NULL);
  _thunar_return_val_if_fail (g_list_length (source_file_list) == g_list_length (target_file_list), NULL);

  job = thunar_transfer_job_new (source_file_list, target_file_list, 
                        THUNAR_TRANSFER_JOB_MOVE);

  return THUNAR_JOB (exo_job_launch (EXO_JOB (job)));
}



static gboolean
_thunar_io_jobs_chown (ThunarJob   *job,
                       GValueArray *param_values,
                       GError     **error)
{
  ThunarJobResponse response;
  const gchar      *message;
  GFileInfo        *info;
  gboolean          recursive;
  GError           *err = NULL;
  GList            *file_list;
  GList            *lp;
  gint              uid;
  gint              gid;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (param_values != NULL, FALSE);
  _thunar_return_val_if_fail (param_values->n_values == 4, FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  file_list = g_value_get_boxed (g_value_array_get_nth (param_values, 0));
  uid = g_value_get_int (g_value_array_get_nth (param_values, 1));
  gid = g_value_get_int (g_value_array_get_nth (param_values, 2));
  recursive = g_value_get_boolean (g_value_array_get_nth (param_values, 3));

  _thunar_assert ((uid >= 0 || gid >= 0) && !(uid >= 0 && gid >= 0));

  /* collect the files for the chown operation */
  if (recursive)
    file_list = _tij_collect_nofollow (job, file_list, &err);
  else
    file_list = thunar_g_file_list_copy (file_list);

  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  /* we know the total list of files to process */
  thunar_job_set_total_files (THUNAR_JOB (job), file_list);

  /* change the ownership of all files */
  for (lp = file_list; lp != NULL && err == NULL; lp = lp->next)
    {
      /* update progress information */
      thunar_job_processing_file (THUNAR_JOB (job), lp);

      /* try to query information about the file */
      info = g_file_query_info (lp->data, 
                                G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                exo_job_get_cancellable (EXO_JOB (job)),
                                &err);

      if (err != NULL)
        break;

retry_chown:
      if (uid >= 0)
        {
          /* try to change the owner UID */
          g_file_set_attribute_uint32 (lp->data,
                                       G_FILE_ATTRIBUTE_UNIX_UID, uid,
                                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                       exo_job_get_cancellable (EXO_JOB (job)),
                                       &err);
        }
      else if (gid >= 0)
        {
          /* try to change the owner GID */
          g_file_set_attribute_uint32 (lp->data,
                                       G_FILE_ATTRIBUTE_UNIX_GID, gid,
                                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                       exo_job_get_cancellable (EXO_JOB (job)),
                                       &err);
        }

      /* check if there was a recoverable error */
      if (err != NULL && !exo_job_is_cancelled (EXO_JOB (job)))
        {
          /* generate a useful error message */
          message = G_LIKELY (uid >= 0) ? _("Failed to change the owner of \"%s\": %s") 
                                        : _("Failed to change the group of \"%s\": %s");

          /* ask the user whether to skip/retry this file */
          response = thunar_job_ask_skip (THUNAR_JOB (job), message, 
                                          g_file_info_get_display_name (info),
                                          err->message);

          /* clear the error */
          g_clear_error (&err);

          /* check whether to retry */
          if (response == THUNAR_JOB_RESPONSE_RETRY)
            goto retry_chown;
        }

      /* release file information */
      g_object_unref (info);
    }

  /* release the file list */
  thunar_g_file_list_free (file_list);

  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}



ThunarJob *
thunar_io_jobs_change_group (GFile    *file,
                             guint32   gid,
                             gboolean  recursive)
{
  GList file_list;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);

  file_list.data = g_object_ref (file);
  file_list.next = NULL; 
  file_list.prev = NULL;
  
  return thunar_simple_job_launch (_thunar_io_jobs_chown, 4,
                                   THUNAR_TYPE_G_FILE_LIST, &file_list,
                                   G_TYPE_INT, -1,
                                   G_TYPE_INT, (gint) gid,
                                   G_TYPE_BOOLEAN, recursive);

  g_object_unref (file_list.data);
}



static gboolean
_thunar_io_jobs_chmod (ThunarJob   *job,
                       GValueArray *param_values,
                       GError     **error)
{
  ThunarJobResponse response;
  GFileInfo        *info;
  gboolean          recursive;
  GError           *err = NULL;
  GList            *file_list;
  GList            *lp;
  ThunarFileMode    dir_mask;
  ThunarFileMode    dir_mode;
  ThunarFileMode    file_mask;
  ThunarFileMode    file_mode;
  ThunarFileMode    mask;
  ThunarFileMode    mode;
  ThunarFileMode    old_mode;
  ThunarFileMode    new_mode;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (param_values != NULL, FALSE);
  _thunar_return_val_if_fail (param_values->n_values == 6, FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  file_list = g_value_get_boxed (g_value_array_get_nth (param_values, 0));
  dir_mask = g_value_get_flags (g_value_array_get_nth (param_values, 1));
  dir_mode = g_value_get_flags (g_value_array_get_nth (param_values, 2));
  file_mask = g_value_get_flags (g_value_array_get_nth (param_values, 3));
  file_mode = g_value_get_flags (g_value_array_get_nth (param_values, 4));
  recursive = g_value_get_boolean (g_value_array_get_nth (param_values, 5));

  /* collect the files for the chown operation */
  if (recursive)
    file_list = _tij_collect_nofollow (job, file_list, &err);
  else
    file_list = thunar_g_file_list_copy (file_list);

  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  /* we know the total list of files to process */
  thunar_job_set_total_files (THUNAR_JOB (job), file_list);

  /* change the ownership of all files */
  for (lp = file_list; lp != NULL && err == NULL; lp = lp->next)
    {
      /* update progress information */
      thunar_job_processing_file (THUNAR_JOB (job), lp);

      /* try to query information about the file */
      info = g_file_query_info (lp->data, 
                                G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
                                G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                G_FILE_ATTRIBUTE_UNIX_MODE,
                                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                exo_job_get_cancellable (EXO_JOB (job)),
                                &err);

      if (err != NULL)
        break;

retry_chown:
      /* different actions depending on the type of the file */
      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        {
          mask = dir_mask;
          mode = dir_mode;
        }
      else
        {
          mask = file_mask;
          mode = file_mode;
        }

      /* determine the current mode */
      old_mode = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);

      /* generate the new mode, taking the old mode (which contains file type 
       * information) into account */
      new_mode = ((old_mode & ~mask) | mode) & 07777;

      /* try to change the file mode */
      g_file_set_attribute_uint32 (lp->data,
                                   G_FILE_ATTRIBUTE_UNIX_MODE, new_mode,
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   exo_job_get_cancellable (EXO_JOB (job)),
                                   &err);

      /* check if there was a recoverable error */
      if (err != NULL && !exo_job_is_cancelled (EXO_JOB (job)))
        {
          /* ask the user whether to skip/retry this file */
          response = thunar_job_ask_skip (job,
                                          _("Failed to change the permissions of \"%s\": %s"), 
                                          g_file_info_get_display_name (info),
                                          err->message);

          /* clear the error */
          g_clear_error (&err);

          /* check whether to retry */
          if (response == THUNAR_JOB_RESPONSE_RETRY)
            goto retry_chown;
        }

      /* release file information */
      g_object_unref (info);
    }

  /* release the file list */
  thunar_g_file_list_free (file_list);

  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }
  else
    {
      return TRUE;
    }
  return TRUE;
}



ThunarJob *
thunar_io_jobs_change_mode (GFile         *file,
                            ThunarFileMode dir_mask,
                            ThunarFileMode dir_mode,
                            ThunarFileMode file_mask,
                            ThunarFileMode file_mode,
                            gboolean       recursive)
{
  GList file_list;

  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);

  file_list.data = g_object_ref (file);
  file_list.next = NULL; 
  file_list.prev = NULL;
  
  return thunar_simple_job_launch (_thunar_io_jobs_chmod, 6,
                                   THUNAR_TYPE_G_FILE_LIST, &file_list,
                                   THUNAR_TYPE_FILE_MODE, dir_mask,
                                   THUNAR_TYPE_FILE_MODE, dir_mode,
                                   THUNAR_TYPE_FILE_MODE, file_mask,
                                   THUNAR_TYPE_FILE_MODE, file_mode,
                                   G_TYPE_BOOLEAN, recursive);

  g_object_unref (file_list.data);
}



static gboolean
_thunar_io_jobs_ls (ThunarJob   *job,
                    GValueArray *param_values,
                    GError     **error)
{
  ThunarFile *file;
  GError     *err = NULL;
  GFile      *directory;
  GList      *file_list = NULL;
  GList      *lp;
  GList      *path_list;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (param_values != NULL, FALSE);
  _thunar_return_val_if_fail (param_values->n_values == 1, FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return FALSE;

  /* determine the directory to list */
  directory = g_value_get_object (g_value_array_get_nth (param_values, 0));

  /* make sure the object is valid */
  _thunar_assert (G_IS_FILE (directory));

  /* collect directory contents (non-recursively) */
  path_list = thunar_io_scan_directory (job, directory, 
                                        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, 
                                        FALSE, &err);

  /* turn the GFile list into a ThunarFile list */
  for (lp = g_list_last (path_list); 
       err == NULL && !exo_job_is_cancelled (EXO_JOB (job)) && lp != NULL; 
       lp = lp->prev)
    {
      file = thunar_file_get (lp->data, &err);
      if (G_LIKELY (file != NULL))
        file_list = g_list_prepend (file_list, file);
    }

  /* free the GFile list */
  thunar_g_file_list_free (path_list);

  /* abort on errors or cancellation */
  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }
  else if (exo_job_set_error_if_cancelled (EXO_JOB (job), &err))
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  /* check if we have any files to report */
  if (G_LIKELY (file_list != NULL))
    {
      /* emit the "files-ready" signal */
      if (!thunar_job_files_ready (THUNAR_JOB (job), file_list))
        {
          /* none of the handlers took over the file list, so it's up to us
           * to destroy it */
          thunar_file_list_free (file_list);
        }
    }
  
  /* there should be no errors here */
  _thunar_assert (err == NULL);

  /* propagate cancellation error */
  if (exo_job_set_error_if_cancelled (EXO_JOB (job), &err))
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  return TRUE;
}



ThunarJob *
thunar_io_jobs_list_directory (GFile *directory)
{
  _thunar_return_val_if_fail (G_IS_FILE (directory), NULL);
  
  return thunar_simple_job_launch (_thunar_io_jobs_ls, 1, G_TYPE_FILE, directory);
}



gboolean
_thunar_io_jobs_rename_notify (ThunarFile *file)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  /* tell the associated folder that the file was renamed */
  thunarx_file_info_renamed (THUNARX_FILE_INFO (file));

  /* emit the file changed signal */
  thunar_file_changed (file);

  return FALSE;
}



gboolean
_thunar_io_jobs_rename (ThunarJob   *job,
                        GValueArray *param_values,
                        GError     **error)
{
  const gchar *display_name;
  ThunarFile  *file;
  GError      *err = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_JOB (job), FALSE);
  _thunar_return_val_if_fail (param_values != NULL, FALSE);
  _thunar_return_val_if_fail (param_values->n_values == 2, FALSE);
  _thunar_return_val_if_fail (G_VALUE_HOLDS (&param_values->values[0], THUNAR_TYPE_FILE), FALSE);
  _thunar_return_val_if_fail (G_VALUE_HOLDS_STRING (&param_values->values[1]), FALSE);
  _thunar_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return FALSE;

  /* determine the file and display name */
  file = g_value_get_object (g_value_array_get_nth (param_values, 0));
  display_name = g_value_get_string (g_value_array_get_nth (param_values, 1));

  /* try to rename the file */
  if (thunar_file_rename (file, display_name, exo_job_get_cancellable (EXO_JOB (job)), TRUE, &err))
    {
      exo_job_send_to_mainloop (EXO_JOB (job), 
                                (GSourceFunc) _thunar_io_jobs_rename_notify, 
                                g_object_ref (file), g_object_unref);
    }

  /* abort on errors or cancellation */
  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  return TRUE;
}



ThunarJob *
thunar_io_jobs_rename_file (ThunarFile  *file,
                            const gchar *display_name)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (g_utf8_validate (display_name, -1, NULL), NULL);

  return thunar_simple_job_launch (_thunar_io_jobs_rename, 2, 
                                   THUNAR_TYPE_FILE, file, 
                                   G_TYPE_STRING, display_name);
}