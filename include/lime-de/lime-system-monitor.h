/*
 * LiMe System Monitor Header
 * Comprehensive system resource monitoring - CPU, memory, disk, processes
 */

#ifndef __LIME_SYSTEM_MONITOR_H__
#define __LIME_SYSTEM_MONITOR_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define LIME_TYPE_SYSTEM_MONITOR (lime_system_monitor_get_type())
#define LIME_SYSTEM_MONITOR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_SYSTEM_MONITOR, LiMeSystemMonitor))
#define LIME_IS_SYSTEM_MONITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_SYSTEM_MONITOR))

typedef struct _LiMeSystemMonitor LiMeSystemMonitor;
typedef struct _LiMeSystemMonitorClass LiMeSystemMonitorClass;

typedef struct {
  pid_t pid;
  gchar *process_name;
  gchar *command;
  gint cpu_usage;
  guint64 memory_usage;
  guint64 virtual_memory;
  gint priority;
  gchar *state;
  gint num_threads;
  gchar *user;
} LiMeProcess;

typedef struct {
  gdouble user_time;
  gdouble system_time;
  gdouble idle_time;
  gdouble iowait_time;
  gdouble total_usage;
  gint core_count;
} LiMeCpuInfo;

typedef struct {
  guint64 total;
  guint64 available;
  guint64 free;
  guint64 buffers;
  guint64 cached;
  gdouble usage_percentage;
} LiMeMemoryInfo;

typedef struct {
  gchar *mount_point;
  gchar *filesystem;
  guint64 total_size;
  guint64 used_size;
  guint64 free_size;
  gdouble usage_percentage;
  gint inodes_used;
  gint inodes_free;
} LiMeDiskInfo;

typedef struct {
  gchar *interface_name;
  guint64 bytes_sent;
  guint64 bytes_received;
  guint64 packets_sent;
  guint64 packets_received;
  guint64 errors;
  guint64 dropped;
} LiMeNetworkStats;

GType lime_system_monitor_get_type(void);

LiMeSystemMonitor * lime_system_monitor_new(void);

/* CPU monitoring */
LiMeCpuInfo * lime_system_monitor_get_cpu_info(LiMeSystemMonitor *monitor);
gdouble lime_system_monitor_get_cpu_usage(LiMeSystemMonitor *monitor);
gint lime_system_monitor_get_cpu_core_count(LiMeSystemMonitor *monitor);
gdouble * lime_system_monitor_get_per_core_usage(LiMeSystemMonitor *monitor,
                                                  gint *core_count);
gdouble lime_system_monitor_get_cpu_frequency(LiMeSystemMonitor *monitor);
gchar * lime_system_monitor_get_cpu_model(LiMeSystemMonitor *monitor);

/* Memory monitoring */
LiMeMemoryInfo * lime_system_monitor_get_memory_info(LiMeSystemMonitor *monitor);
guint64 lime_system_monitor_get_total_memory(LiMeSystemMonitor *monitor);
guint64 lime_system_monitor_get_used_memory(LiMeSystemMonitor *monitor);
guint64 lime_system_monitor_get_available_memory(LiMeSystemMonitor *monitor);
gdouble lime_system_monitor_get_memory_usage_percentage(LiMeSystemMonitor *monitor);

/* Disk monitoring */
GList * lime_system_monitor_get_disk_info(LiMeSystemMonitor *monitor);
LiMeDiskInfo * lime_system_monitor_get_disk_info_for_path(LiMeSystemMonitor *monitor,
                                                           const gchar *path);
guint64 lime_system_monitor_get_disk_total_size(LiMeSystemMonitor *monitor,
                                                 const gchar *disk);
guint64 lime_system_monitor_get_disk_used_size(LiMeSystemMonitor *monitor,
                                                const gchar *disk);
guint64 lime_system_monitor_get_disk_free_size(LiMeSystemMonitor *monitor,
                                                const gchar *disk);

/* Process monitoring */
GList * lime_system_monitor_get_all_processes(LiMeSystemMonitor *monitor);
GList * lime_system_monitor_get_processes_by_cpu(LiMeSystemMonitor *monitor,
                                                  gint limit);
GList * lime_system_monitor_get_processes_by_memory(LiMeSystemMonitor *monitor,
                                                     gint limit);
LiMeProcess * lime_system_monitor_get_process_info(LiMeSystemMonitor *monitor,
                                                    pid_t pid);
gint lime_system_monitor_get_process_count(LiMeSystemMonitor *monitor);
gint lime_system_monitor_get_thread_count(LiMeSystemMonitor *monitor);

/* Process control */
gboolean lime_system_monitor_kill_process(LiMeSystemMonitor *monitor, pid_t pid);
gboolean lime_system_monitor_suspend_process(LiMeSystemMonitor *monitor, pid_t pid);
gboolean lime_system_monitor_resume_process(LiMeSystemMonitor *monitor, pid_t pid);
gboolean lime_system_monitor_set_process_priority(LiMeSystemMonitor *monitor,
                                                   pid_t pid,
                                                   gint priority);

/* Network monitoring */
GList * lime_system_monitor_get_network_stats(LiMeSystemMonitor *monitor);
LiMeNetworkStats * lime_system_monitor_get_network_interface_stats(LiMeSystemMonitor *monitor,
                                                                    const gchar *interface);
guint64 lime_system_monitor_get_total_bytes_sent(LiMeSystemMonitor *monitor);
guint64 lime_system_monitor_get_total_bytes_received(LiMeSystemMonitor *monitor);

/* System uptime and load */
gint64 lime_system_monitor_get_system_uptime(LiMeSystemMonitor *monitor);
gdouble * lime_system_monitor_get_load_average(LiMeSystemMonitor *monitor);
gint lime_system_monitor_get_running_processes(LiMeSystemMonitor *monitor);

/* Monitoring control */
gboolean lime_system_monitor_start_monitoring(LiMeSystemMonitor *monitor,
                                               guint interval_ms);
gboolean lime_system_monitor_stop_monitoring(LiMeSystemMonitor *monitor);
gboolean lime_system_monitor_is_monitoring(LiMeSystemMonitor *monitor);

/* Alerts and notifications */
gboolean lime_system_monitor_set_cpu_alert_threshold(LiMeSystemMonitor *monitor,
                                                      gdouble threshold);
gboolean lime_system_monitor_set_memory_alert_threshold(LiMeSystemMonitor *monitor,
                                                        gdouble threshold);
gboolean lime_system_monitor_set_disk_alert_threshold(LiMeSystemMonitor *monitor,
                                                      gdouble threshold);

/* System information */
gchar * lime_system_monitor_get_system_info(LiMeSystemMonitor *monitor);
gchar * lime_system_monitor_get_kernel_version(LiMeSystemMonitor *monitor);
gchar * lime_system_monitor_get_hostname(LiMeSystemMonitor *monitor);

G_END_DECLS

#endif /* __LIME_SYSTEM_MONITOR_H__ */
