/* See LICENSE file for license and copyright information */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>

#include "bookmarks.h"
#include "completion.h"
#include "utils.h"

#include <girara/session.h>
#include <girara/settings.h>
#include <girara/completion.h>
#include <girara/utils.h>
#include <girara/datastructures.h>

static int
compare_case_insensitive(const char* str1, const char* str2)
{
  char* ustr1 = g_utf8_casefold(str1, -1);
  char* ustr2 = g_utf8_casefold(str2, -1);
  int res = g_utf8_collate(ustr1, ustr2);
  g_free(ustr1);
  g_free(ustr2);
  return res;
}

static girara_list_t*
list_files(zathura_t* zathura, const char* current_path, const char* current_file, int current_file_length, bool is_dir)
{
  if (zathura == NULL || zathura->ui.session == NULL || current_path == NULL) {
    return NULL;
  }

  /* read directory */
  GDir* dir = g_dir_open(current_path, 0, NULL);
  if (dir == NULL) {
    return NULL;
  }

  girara_list_t* res = girara_sorted_list_new2((girara_compare_function_t)compare_case_insensitive,
      (girara_free_function_t)g_free);

  bool show_hidden = false;
  girara_setting_get(zathura->ui.session, "show-hidden", &show_hidden);

  /* read files */
  char* name = NULL;
  while ((name = (char*) g_dir_read_name(dir)) != NULL) {
    char* e_name   = g_filename_display_name(name);
    if (e_name == NULL) {
      goto error_free;
    }

    int e_length = strlen(e_name);

    if (show_hidden == false && e_name[0] == '.') {
      g_free(e_name);
      continue;
    }

    if ((current_file_length > e_length) || strncmp(current_file, e_name, current_file_length)) {
      g_free(e_name);
      continue;
    }

    char* tmp = "/";
    if (is_dir == true || g_strcmp0(current_path, "/") == 0) {
      tmp = "";
    };

    char* full_path = g_strdup_printf("%s%s%s", current_path, tmp, e_name);
    if (full_path == NULL) {
      g_free(e_name);
      goto error_free;
    }

    if (g_file_test(full_path, G_FILE_TEST_IS_DIR) == true) {
      char* tmp_path = full_path;
      full_path = g_strdup_printf("%s/", full_path);
      g_free(tmp_path);
      girara_list_append(res, full_path);
    } else if (file_valid_extension(zathura, full_path) == true) {
      girara_list_append(res, full_path);
    } else {
      g_free(full_path);
    }
    g_free(e_name);
  }

  g_dir_close(dir);
  return res;

error_free:
  girara_list_free(res);
  return NULL;
}

girara_completion_t*
cc_open(girara_session_t* session, const char* input)
{
  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(session->global.data != NULL, NULL);
  zathura_t* zathura = session->global.data;

  girara_completion_t* completion  = girara_completion_init();
  girara_completion_group_t* group = girara_completion_group_create(session, NULL);

  gchar* path         = NULL;
  gchar* current_path = NULL;

  if (completion == NULL || group == NULL) {
    goto error_free;
  }

  path = girara_fix_path(input);
  if (path == NULL) {
    goto error_free;
  }

  /* If the path does not begin with a slash we update the path with the current
   * working directory */
  if (strlen(path) == 0 || path[0] != '/') {
    long path_max;
#ifdef PATH_MAX
    path_max = PATH_MAX;
#else
    path_max = pathconf(path,_PC_PATH_MAX);
    if (path_max <= 0)
      path_max = 4096;
#endif

    char cwd[path_max];
    if (getcwd(cwd, path_max) == NULL) {
      goto error_free;
    }

    char* tmp_path = g_strdup_printf("%s/%s", cwd, path);
    if (tmp_path == NULL) {
      goto error_free;
    }

    g_free(path);
    path = tmp_path;
  }

  /* Append a slash if the given argument is a directory */
  bool is_dir = (path[strlen(path) - 1] == '/') ? true : false;
  if ((g_file_test(path, G_FILE_TEST_IS_DIR) == TRUE) && !is_dir) {
    char* tmp_path = g_strdup_printf("%s/", path);
    if (tmp_path == NULL) {
      goto error_free;
    }

    g_free(path);
    path = tmp_path;
    is_dir = true;
  }

  /* get current path */
  char* tmp    = g_strdup(path);
  current_path = is_dir ? g_strdup(tmp) : g_strdup(dirname(tmp));
  g_free(tmp);

  /* get current file */
  gchar* current_file     = is_dir ? "" : basename(path);
  int current_file_length = strlen(current_file);

  /* read directory */
  if (g_file_test(current_path, G_FILE_TEST_IS_DIR) == TRUE) {
    girara_list_t* names = list_files(zathura, current_path, current_file, current_file_length, is_dir);
    if (!names) {
      goto error_free;
    }

    GIRARA_LIST_FOREACH(names, const char*, iter, file)
      girara_completion_group_add_element(group, file, NULL);
    GIRARA_LIST_FOREACH_END(names, const char*, iter, file);
    girara_list_free(names);
  }

  g_free(path);
  g_free(current_path);

  girara_completion_add_group(completion, group);

  return completion;

error_free:

  if (completion) {
    girara_completion_free(completion);
  }
  if (group) {
    girara_completion_group_free(group);
  }

  g_free(current_path);
  g_free(path);

  return NULL;
}

girara_completion_t*
cc_bookmarks(girara_session_t* session, const char* input)
{
  if (input == NULL) {
    return NULL;
  }

  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(session->global.data != NULL, NULL);
  zathura_t* zathura = session->global.data;

  girara_completion_t* completion  = girara_completion_init();
  girara_completion_group_t* group = girara_completion_group_create(session, NULL);

  if (completion == NULL || group == NULL) {
    goto error_free;
  }

  const size_t input_length = strlen(input);
  GIRARA_LIST_FOREACH(zathura->bookmarks.bookmarks, zathura_bookmark_t*, iter, bookmark)
    if (input_length <= strlen(bookmark->id) && !strncmp(input, bookmark->id, input_length)) {
      gchar* paged = g_strdup_printf("Page %d", bookmark->page);
      girara_completion_group_add_element(group, bookmark->id, paged);
      g_free(paged);
    }
  GIRARA_LIST_FOREACH_END(zathura->bookmarks.bookmarks, zathura_bookmark_t*, iter, bookmark);

  girara_completion_add_group(completion, group);

  return completion;

error_free:

  if (completion) {
    girara_completion_free(completion);
  }

  if (group) {
    girara_completion_group_free(group);
  }

  return NULL;
}

girara_completion_t*
cc_export(girara_session_t* session, const char* input)
{
  if (input == NULL) {
    return NULL;
  }

  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(session->global.data != NULL, NULL);
  zathura_t* zathura = session->global.data;

  girara_completion_t* completion  = girara_completion_init();
  girara_completion_group_t* group = girara_completion_group_create(session, NULL);

  if (completion == NULL || group == NULL) {
    goto error_free;
  }

  const size_t input_length = strlen(input);
  girara_list_t* attachments = zathura_document_attachments_get(zathura->document, NULL);
  if (attachments == NULL) {
    goto error_free;
  }

  GIRARA_LIST_FOREACH(attachments, const char*, iter, attachment)
    if (input_length <= strlen(attachment) && !strncmp(input, attachment, input_length)) {
      girara_completion_group_add_element(group, attachment, NULL);
    }
  GIRARA_LIST_FOREACH_END(zathura->bookmarks.bookmarks, zathura_bookmark_t*, iter, bookmark);

  girara_completion_add_group(completion, group);
  girara_list_free(attachments);
  return completion;

error_free:

  if (completion) {
    girara_completion_free(completion);
  }

  if (group) {
    girara_completion_group_free(group);
  }

  return NULL;
}
