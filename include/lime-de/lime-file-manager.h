/*
 * LiMe File Manager Header
 * Advanced file management with preview, search, tagging, and batch operations
 */

#ifndef __LIME_FILE_MANAGER_H__
#define __LIME_FILE_MANAGER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define LIME_TYPE_FILE_MANAGER (lime_file_manager_get_type())
#define LIME_FILE_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_FILE_MANAGER, LiMeFileManager))
#define LIME_IS_FILE_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_FILE_MANAGER))

typedef struct _LiMeFileManager LiMeFileManager;
typedef struct _LiMeFileManagerClass LiMeFileManagerClass;

typedef enum {
  LIME_FILE_TYPE_REGULAR,
  LIME_FILE_TYPE_DIRECTORY,
  LIME_FILE_TYPE_SYMLINK,
  LIME_FILE_TYPE_DEVICE,
  LIME_FILE_TYPE_SOCKET,
  LIME_FILE_TYPE_FIFO,
  LIME_FILE_TYPE_UNKNOWN
} LiMeFileType;

typedef enum {
  LIME_SORT_BY_NAME,
  LIME_SORT_BY_SIZE,
  LIME_SORT_BY_DATE,
  LIME_SORT_BY_TYPE,
  LIME_SORT_BY_MODIFIED
} LiMeSortBy;

typedef enum {
  LIME_VIEW_LIST,
  LIME_VIEW_ICON,
  LIME_VIEW_DETAILED,
  LIME_VIEW_COMPACT
} LiMeViewMode;

typedef struct {
  gchar *path;
  gchar *name;
  gchar *display_name;
  LiMeFileType file_type;
  guint64 size;
  gint64 modified_time;
  gint64 accessed_time;
  gint64 created_time;
  gchar *mime_type;
  gchar *icon_name;
  gint permissions;
  gchar *owner;
  gchar *group;
  gboolean is_hidden;
  gboolean is_symlink;
  gchar *symlink_target;
  GList *tags;
  gchar *checksum;
} LiMeFileInfo;

GType lime_file_manager_get_type(void);

LiMeFileManager * lime_file_manager_new(void);

/* Navigation */
gboolean lime_file_manager_open_directory(LiMeFileManager *manager,
                                          const gchar *directory_path);
gchar * lime_file_manager_get_current_directory(LiMeFileManager *manager);
gboolean lime_file_manager_go_to_parent(LiMeFileManager *manager);
gboolean lime_file_manager_go_to_home(LiMeFileManager *manager);
gboolean lime_file_manager_go_back(LiMeFileManager *manager);
gboolean lime_file_manager_go_forward(LiMeFileManager *manager);

/* File listing */
GList * lime_file_manager_list_files(LiMeFileManager *manager);
GList * lime_file_manager_list_files_recursive(LiMeFileManager *manager);
gint lime_file_manager_get_file_count(LiMeFileManager *manager);

/* File information */
LiMeFileInfo * lime_file_manager_get_file_info(LiMeFileManager *manager,
                                                const gchar *file_path);
gchar * lime_file_manager_get_file_mime_type(LiMeFileManager *manager,
                                              const gchar *file_path);
guint64 lime_file_manager_get_directory_size(LiMeFileManager *manager,
                                              const gchar *directory_path);
gint lime_file_manager_get_file_permissions(LiMeFileManager *manager,
                                             const gchar *file_path);

/* Sorting and filtering */
void lime_file_manager_set_sort_by(LiMeFileManager *manager, LiMeSortBy sort_by);
void lime_file_manager_set_sort_ascending(LiMeFileManager *manager, gboolean ascending);
void lime_file_manager_show_hidden_files(LiMeFileManager *manager, gboolean show);
void lime_file_manager_set_view_mode(LiMeFileManager *manager, LiMeViewMode mode);

/* File operations */
gboolean lime_file_manager_create_file(LiMeFileManager *manager,
                                        const gchar *file_path,
                                        const gchar *content);
gboolean lime_file_manager_create_directory(LiMeFileManager *manager,
                                             const gchar *directory_path);
gboolean lime_file_manager_copy_file(LiMeFileManager *manager,
                                      const gchar *source_path,
                                      const gchar *destination_path);
gboolean lime_file_manager_move_file(LiMeFileManager *manager,
                                      const gchar *source_path,
                                      const gchar *destination_path);
gboolean lime_file_manager_rename_file(LiMeFileManager *manager,
                                        const gchar *file_path,
                                        const gchar *new_name);
gboolean lime_file_manager_delete_file(LiMeFileManager *manager,
                                        const gchar *file_path);
gboolean lime_file_manager_delete_file_permanent(LiMeFileManager *manager,
                                                  const gchar *file_path);

/* Batch operations */
gboolean lime_file_manager_copy_multiple(LiMeFileManager *manager,
                                          const gchar * const *source_paths,
                                          const gchar *destination_path);
gboolean lime_file_manager_move_multiple(LiMeFileManager *manager,
                                          const gchar * const *source_paths,
                                          const gchar *destination_path);
gboolean lime_file_manager_delete_multiple(LiMeFileManager *manager,
                                            const gchar * const *file_paths);
gboolean lime_file_manager_batch_rename(LiMeFileManager *manager,
                                         const gchar * const *file_paths,
                                         const gchar *pattern);
gboolean lime_file_manager_batch_set_permissions(LiMeFileManager *manager,
                                                  const gchar * const *file_paths,
                                                  gint permissions);

/* Search functionality */
GList * lime_file_manager_search_by_name(LiMeFileManager *manager,
                                          const gchar *search_pattern);
GList * lime_file_manager_search_by_content(LiMeFileManager *manager,
                                             const gchar *search_term);
GList * lime_file_manager_search_by_type(LiMeFileManager *manager,
                                          const gchar *mime_type);
GList * lime_file_manager_search_by_size(LiMeFileManager *manager,
                                          guint64 min_size,
                                          guint64 max_size);
GList * lime_file_manager_search_by_date(LiMeFileManager *manager,
                                          gint64 start_date,
                                          gint64 end_date);

/* File preview */
gboolean lime_file_manager_generate_thumbnail(LiMeFileManager *manager,
                                               const gchar *file_path,
                                               gint width,
                                               gint height);
gchar * lime_file_manager_get_text_preview(LiMeFileManager *manager,
                                            const gchar *file_path,
                                            gint max_lines);

/* Tagging system */
gboolean lime_file_manager_add_tag(LiMeFileManager *manager,
                                    const gchar *file_path,
                                    const gchar *tag);
gboolean lime_file_manager_remove_tag(LiMeFileManager *manager,
                                       const gchar *file_path,
                                       const gchar *tag);
GList * lime_file_manager_get_tags(LiMeFileManager *manager,
                                    const gchar *file_path);
GList * lime_file_manager_search_by_tag(LiMeFileManager *manager,
                                         const gchar *tag);

/* File checksums and integrity */
gchar * lime_file_manager_calculate_checksum(LiMeFileManager *manager,
                                              const gchar *file_path);
gboolean lime_file_manager_verify_checksum(LiMeFileManager *manager,
                                            const gchar *file_path,
                                            const gchar *expected_checksum);

/* Compression */
gboolean lime_file_manager_compress_files(LiMeFileManager *manager,
                                           const gchar * const *file_paths,
                                           const gchar *archive_path);
gboolean lime_file_manager_extract_archive(LiMeFileManager *manager,
                                            const gchar *archive_path,
                                            const gchar *destination_path);

/* File associations */
gboolean lime_file_manager_open_file(LiMeFileManager *manager,
                                      const gchar *file_path);
gboolean lime_file_manager_set_default_application(LiMeFileManager *manager,
                                                    const gchar *mime_type,
                                                    const gchar *application);
gchar * lime_file_manager_get_default_application(LiMeFileManager *manager,
                                                   const gchar *mime_type);

/* Bookmarks and favorites */
gboolean lime_file_manager_add_bookmark(LiMeFileManager *manager,
                                         const gchar *path,
                                         const gchar *label);
gboolean lime_file_manager_remove_bookmark(LiMeFileManager *manager,
                                            const gchar *path);
GList * lime_file_manager_get_bookmarks(LiMeFileManager *manager);

/* Configuration */
gboolean lime_file_manager_save_config(LiMeFileManager *manager);
gboolean lime_file_manager_load_config(LiMeFileManager *manager);

G_END_DECLS

#endif /* __LIME_FILE_MANAGER_H__ */
