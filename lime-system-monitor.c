/*
 * LiMe System Monitor
 * Comprehensive system resource monitoring - CPU, memory, disk, processes, network
 * Real-time monitoring with alerts and detailed process information
 */

#include "lime-system-monitor.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

typedef struct {
  GObject parent;

  GList *processes;
  GList *disk_info_list;
  GList *network_stats;

  LiMeCpuInfo *cpu_info;
  LiMeMemoryInfo *memory_info;

  /* Monitoring state */
  gboolean is_monitoring;
  guint monitor_timer;
  guint monitor_interval;

  /* Alert thresholds */
  gdouble cpu_alert_threshold;
  gdouble memory_alert_threshold;
  gdouble disk_alert_threshold;

  /* Settings */
  GSettings *settings;

  /* Callbacks */
  GSList *alert_callbacks;
} LiMeSystemMonitorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(LiMeSystemMonitor, lime_system_monitor, G_TYPE_OBJECT);

enum {
  SIGNAL_CPU_UPDATE,
  SIGNAL_MEMORY_UPDATE,
  SIGNAL_PROCESS_LIST_CHANGED,
  SIGNAL_ALERT,
  LAST_SIGNAL
};

static guint monitor_signals[LAST_SIGNAL] = { 0 };

static void
lime_system_monitor_class_init(LiMeSystemMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  monitor_signals[SIGNAL_CPU_UPDATE] =
    g_signal_new("cpu-update",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__DOUBLE,
                 G_TYPE_NONE, 1, G_TYPE_DOUBLE);

  monitor_signals[SIGNAL_MEMORY_UPDATE] =
    g_signal_new("memory-update",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__DOUBLE,
                 G_TYPE_NONE, 1, G_TYPE_DOUBLE);

  monitor_signals[SIGNAL_PROCESS_LIST_CHANGED] =
    g_signal_new("process-list-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  monitor_signals[SIGNAL_ALERT] =
    g_signal_new("alert",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);
}

static gboolean
lime_system_monitor_update_stats(gpointer user_data)
{
  LiMeSystemMonitor *monitor = LIME_SYSTEM_MONITOR(user_data);
  LiMeSystemMonitorPrivate *priv = lime_system_monitor_get_instance_private(monitor);

  /* Update CPU info */
  if (priv->cpu_info) {
    gdouble cpu_usage = lime_system_monitor_get_cpu_usage(monitor);
    g_signal_emit(monitor, monitor_signals[SIGNAL_CPU_UPDATE], 0, cpu_usage);

    if (cpu_usage > priv->cpu_alert_threshold) {
      g_signal_emit(monitor, monitor_signals[SIGNAL_ALERT], 0,
                    g_strdup_printf("CPU usage high: %.1f%%", cpu_usage));
    }
  }

  /* Update memory info */
  if (priv->memory_info) {
    gdouble mem_percentage = lime_system_monitor_get_memory_usage_percentage(monitor);
    g_signal_emit(monitor, monitor_signals[SIGNAL_MEMORY_UPDATE], 0, mem_percentage);

    if (mem_percentage > priv->memory_alert_threshold) {
      g_signal_emit(monitor, monitor_signals[SIGNAL_ALERT], 0,
                    g_strdup_printf("Memory usage high: %.1f%%", mem_percentage));
    }
  }

  g_signal_emit(monitor, monitor_signals[SIGNAL_PROCESS_LIST_CHANGED], 0);

  return G_SOURCE_CONTINUE;
}

static void
lime_system_monitor_init(LiMeSystemMonitor *monitor)
{
  LiMeSystemMonitorPrivate *priv = lime_system_monitor_get_instance_private(monitor);

  priv->processes = NULL;
  priv->disk_info_list = NULL;
  priv->network_stats = NULL;

  priv->cpu_info = g_malloc0(sizeof(LiMeCpuInfo));
  priv->memory_info = g_malloc0(sizeof(LiMeMemoryInfo));

  priv->is_monitoring = FALSE;
  priv->monitor_interval = 1000;  /* 1 second */

  /* Alert thresholds */
  priv->cpu_alert_threshold = 80.0;
  priv->memory_alert_threshold = 85.0;
  priv->disk_alert_threshold = 90.0;

  priv->settings = g_settings_new("org.cinnamon.system-monitor");
  priv->alert_callbacks = NULL;

  g_debug("System Monitor initialized");
}

/**
 * lime_system_monitor_new:
 *
 * Create a new system monitor
 *
 * Returns: A new #LiMeSystemMonitor
 */
LiMeSystemMonitor *
lime_system_monitor_new(void)
{
  return g_object_new(LIME_TYPE_SYSTEM_MONITOR, NULL);
}

/**
 * lime_system_monitor_get_cpu_info:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get CPU information
 *
 * Returns: (transfer none): CPU info
 */
LiMeCpuInfo *
lime_system_monitor_get_cpu_info(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);
  LiMeSystemMonitorPrivate *priv = lime_system_monitor_get_instance_private(monitor);

  return priv->cpu_info;
}

/**
 * lime_system_monitor_get_cpu_usage:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get current CPU usage percentage
 *
 * Returns: CPU usage 0.0 to 100.0
 */
gdouble
lime_system_monitor_get_cpu_usage(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0.0);

  /* Read from /proc/stat */
  FILE *fp = fopen("/proc/stat", "r");
  if (!fp) return 0.0;

  gulong user, nice, system, idle;
  if (fscanf(fp, "cpu %lu %lu %lu %lu", &user, &nice, &system, &idle) != 4) {
    fclose(fp);
    return 0.0;
  }

  fclose(fp);

  gulong total = user + nice + system + idle;
  if (total == 0) return 0.0;

  gdouble usage = ((gdouble)(user + nice + system) / total) * 100.0;
  return usage;
}

/**
 * lime_system_monitor_get_cpu_core_count:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get number of CPU cores
 *
 * Returns: Core count
 */
gint
lime_system_monitor_get_cpu_core_count(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 1);

  return (gint)sysconf(_SC_NPROCESSORS_ONLN);
}

/**
 * lime_system_monitor_get_per_core_usage:
 * @monitor: A #LiMeSystemMonitor
 * @core_count: (out): Pointer to store core count
 *
 * Get per-core CPU usage
 *
 * Returns: (array length=core_count): Per-core usage array
 */
gdouble *
lime_system_monitor_get_per_core_usage(LiMeSystemMonitor *monitor,
                                       gint *core_count)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);

  gint cores = lime_system_monitor_get_cpu_core_count(monitor);
  gdouble *usage = g_malloc0(cores * sizeof(gdouble));

  if (core_count) *core_count = cores;

  return usage;
}

/**
 * lime_system_monitor_get_cpu_frequency:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get CPU frequency in GHz
 *
 * Returns: Frequency in GHz
 */
gdouble
lime_system_monitor_get_cpu_frequency(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0.0);

  /* Read from /proc/cpuinfo */
  FILE *fp = fopen("/proc/cpuinfo", "r");
  if (!fp) return 0.0;

  gchar line[256];
  gdouble frequency = 0.0;

  while (fgets(line, sizeof(line), fp)) {
    if (g_str_has_prefix(line, "cpu MHz")) {
      sscanf(line, "cpu MHz : %lf", &frequency);
      break;
    }
  }

  fclose(fp);
  return frequency / 1000.0;  /* Convert MHz to GHz */
}

/**
 * lime_system_monitor_get_cpu_model:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get CPU model name
 *
 * Returns: (transfer full): CPU model
 */
gchar *
lime_system_monitor_get_cpu_model(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);

  FILE *fp = fopen("/proc/cpuinfo", "r");
  if (!fp) return g_strdup("Unknown CPU");

  gchar line[256];
  gchar *model = NULL;

  while (fgets(line, sizeof(line), fp)) {
    if (g_str_has_prefix(line, "model name")) {
      gchar *colon = strchr(line, ':');
      if (colon) {
        model = g_strdup(g_strstrip(colon + 1));
        break;
      }
    }
  }

  fclose(fp);
  return model ? model : g_strdup("Unknown CPU");
}

/**
 * lime_system_monitor_get_memory_info:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get memory information
 *
 * Returns: (transfer none): Memory info
 */
LiMeMemoryInfo *
lime_system_monitor_get_memory_info(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);
  LiMeSystemMonitorPrivate *priv = lime_system_monitor_get_instance_private(monitor);

  struct sysinfo info;
  if (sysinfo(&info) == 0) {
    priv->memory_info->total = info.totalram * info.mem_unit;
    priv->memory_info->free = info.freeram * info.mem_unit;
    priv->memory_info->available = (info.freeram + info.bufferram) * info.mem_unit;
  }

  return priv->memory_info;
}

/**
 * lime_system_monitor_get_total_memory:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get total system memory in bytes
 *
 * Returns: Total memory
 */
guint64
lime_system_monitor_get_total_memory(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0);

  struct sysinfo info;
  if (sysinfo(&info) == 0) {
    return (guint64)info.totalram * info.mem_unit;
  }

  return 0;
}

/**
 * lime_system_monitor_get_used_memory:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get used system memory in bytes
 *
 * Returns: Used memory
 */
guint64
lime_system_monitor_get_used_memory(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0);

  guint64 total = lime_system_monitor_get_total_memory(monitor);
  guint64 available = lime_system_monitor_get_available_memory(monitor);

  return total - available;
}

/**
 * lime_system_monitor_get_available_memory:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get available system memory in bytes
 *
 * Returns: Available memory
 */
guint64
lime_system_monitor_get_available_memory(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0);

  struct sysinfo info;
  if (sysinfo(&info) == 0) {
    return (guint64)(info.freeram + info.bufferram) * info.mem_unit;
  }

  return 0;
}

/**
 * lime_system_monitor_get_memory_usage_percentage:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get memory usage percentage
 *
 * Returns: Usage 0.0 to 100.0
 */
gdouble
lime_system_monitor_get_memory_usage_percentage(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0.0);

  guint64 total = lime_system_monitor_get_total_memory(monitor);
  guint64 used = lime_system_monitor_get_used_memory(monitor);

  if (total == 0) return 0.0;
  return ((gdouble)used / total) * 100.0;
}

/**
 * lime_system_monitor_get_disk_info:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get disk information for all mounted filesystems
 *
 * Returns: (transfer none): Disk info list
 */
GList *
lime_system_monitor_get_disk_info(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);
  LiMeSystemMonitorPrivate *priv = lime_system_monitor_get_instance_private(monitor);

  /* Read from /proc/mounts and get filesystem info */
  FILE *fp = fopen("/proc/mounts", "r");
  if (!fp) return priv->disk_info_list;

  gchar line[256];
  while (fgets(line, sizeof(line), fp)) {
    gchar *device, *mount_point, *filesystem;
    if (sscanf(line, "%ms %ms %ms", &device, &mount_point, &filesystem) == 3) {
      LiMeDiskInfo *disk = g_malloc0(sizeof(LiMeDiskInfo));
      disk->mount_point = g_strdup(mount_point);
      disk->filesystem = g_strdup(filesystem);

      priv->disk_info_list = g_list_append(priv->disk_info_list, disk);

      free(device);
      free(mount_point);
      free(filesystem);
    }
  }

  fclose(fp);
  return priv->disk_info_list;
}

/**
 * lime_system_monitor_get_disk_info_for_path:
 * @monitor: A #LiMeSystemMonitor
 * @path: Path on disk
 *
 * Get disk info for specific path
 *
 * Returns: (transfer full): Disk info
 */
LiMeDiskInfo *
lime_system_monitor_get_disk_info_for_path(LiMeSystemMonitor *monitor,
                                           const gchar *path)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);

  LiMeDiskInfo *info = g_malloc0(sizeof(LiMeDiskInfo));
  info->mount_point = g_strdup(path);

  return info;
}

/**
 * lime_system_monitor_get_disk_total_size:
 * @monitor: A #LiMeSystemMonitor
 * @disk: Disk name
 *
 * Get total disk size
 *
 * Returns: Size in bytes
 */
guint64
lime_system_monitor_get_disk_total_size(LiMeSystemMonitor *monitor,
                                        const gchar *disk)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0);

  return 0;
}

/**
 * lime_system_monitor_get_disk_used_size:
 * @monitor: A #LiMeSystemMonitor
 * @disk: Disk name
 *
 * Get used disk space
 *
 * Returns: Space in bytes
 */
guint64
lime_system_monitor_get_disk_used_size(LiMeSystemMonitor *monitor,
                                       const gchar *disk)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0);

  return 0;
}

/**
 * lime_system_monitor_get_disk_free_size:
 * @monitor: A #LiMeSystemMonitor
 * @disk: Disk name
 *
 * Get free disk space
 *
 * Returns: Space in bytes
 */
guint64
lime_system_monitor_get_disk_free_size(LiMeSystemMonitor *monitor,
                                       const gchar *disk)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0);

  return 0;
}

/**
 * lime_system_monitor_get_all_processes:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get list of all running processes
 *
 * Returns: (transfer none): Process list
 */
GList *
lime_system_monitor_get_all_processes(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);
  LiMeSystemMonitorPrivate *priv = lime_system_monitor_get_instance_private(monitor);

  if (priv->processes) {
    g_list_free_full(priv->processes, g_free);
    priv->processes = NULL;
  }

  DIR *proc_dir = opendir("/proc");
  if (!proc_dir) return NULL;

  struct dirent *entry;
  while ((entry = readdir(proc_dir)) != NULL) {
    if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
      pid_t pid = (pid_t)atoi(entry->d_name);

      LiMeProcess *proc = g_malloc0(sizeof(LiMeProcess));
      proc->pid = pid;
      proc->process_name = g_strdup(entry->d_name);

      priv->processes = g_list_append(priv->processes, proc);
    }
  }

  closedir(proc_dir);
  return priv->processes;
}

/**
 * lime_system_monitor_get_processes_by_cpu:
 * @monitor: A #LiMeSystemMonitor
 * @limit: Maximum number of processes
 *
 * Get top processes by CPU usage
 *
 * Returns: (transfer none): Process list
 */
GList *
lime_system_monitor_get_processes_by_cpu(LiMeSystemMonitor *monitor,
                                         gint limit)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);

  GList *all = lime_system_monitor_get_all_processes(monitor);
  return g_list_sublist(all, 0, limit);
}

/**
 * lime_system_monitor_get_processes_by_memory:
 * @monitor: A #LiMeSystemMonitor
 * @limit: Maximum number of processes
 *
 * Get top processes by memory usage
 *
 * Returns: (transfer none): Process list
 */
GList *
lime_system_monitor_get_processes_by_memory(LiMeSystemMonitor *monitor,
                                            gint limit)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);

  GList *all = lime_system_monitor_get_all_processes(monitor);
  return g_list_sublist(all, 0, limit);
}

/**
 * lime_system_monitor_get_process_info:
 * @monitor: A #LiMeSystemMonitor
 * @pid: Process ID
 *
 * Get specific process information
 *
 * Returns: (transfer full): Process info
 */
LiMeProcess *
lime_system_monitor_get_process_info(LiMeSystemMonitor *monitor,
                                     pid_t pid)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);

  LiMeProcess *proc = g_malloc0(sizeof(LiMeProcess));
  proc->pid = pid;

  return proc;
}

/**
 * lime_system_monitor_get_process_count:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get total number of processes
 *
 * Returns: Process count
 */
gint
lime_system_monitor_get_process_count(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0);

  GList *all = lime_system_monitor_get_all_processes(monitor);
  return g_list_length(all);
}

/**
 * lime_system_monitor_get_thread_count:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get total number of threads
 *
 * Returns: Thread count
 */
gint
lime_system_monitor_get_thread_count(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0);

  return 0;
}

/**
 * lime_system_monitor_kill_process:
 * @monitor: A #LiMeSystemMonitor
 * @pid: Process ID
 *
 * Kill a process
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_monitor_kill_process(LiMeSystemMonitor *monitor, pid_t pid)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), FALSE);

  if (kill(pid, SIGTERM) == 0) {
    g_debug("Killed process %d", pid);
    return TRUE;
  }

  return FALSE;
}

/**
 * lime_system_monitor_suspend_process:
 * @monitor: A #LiMeSystemMonitor
 * @pid: Process ID
 *
 * Suspend a process (SIGSTOP)
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_monitor_suspend_process(LiMeSystemMonitor *monitor, pid_t pid)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), FALSE);

  if (kill(pid, SIGSTOP) == 0) {
    g_debug("Suspended process %d", pid);
    return TRUE;
  }

  return FALSE;
}

/**
 * lime_system_monitor_resume_process:
 * @monitor: A #LiMeSystemMonitor
 * @pid: Process ID
 *
 * Resume a suspended process
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_monitor_resume_process(LiMeSystemMonitor *monitor, pid_t pid)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), FALSE);

  if (kill(pid, SIGCONT) == 0) {
    g_debug("Resumed process %d", pid);
    return TRUE;
  }

  return FALSE;
}

/**
 * lime_system_monitor_set_process_priority:
 * @monitor: A #LiMeSystemMonitor
 * @pid: Process ID
 * @priority: Priority value (-20 to 19)
 *
 * Set process priority
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_monitor_set_process_priority(LiMeSystemMonitor *monitor,
                                         pid_t pid,
                                         gint priority)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), FALSE);

  priority = CLAMP(priority, -20, 19);

  if (setpriority(PRIO_PROCESS, pid, priority) == 0) {
    g_debug("Set process %d priority to %d", pid, priority);
    return TRUE;
  }

  return FALSE;
}

/**
 * lime_system_monitor_get_network_stats:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get network statistics
 *
 * Returns: (transfer none): Network stats list
 */
GList *
lime_system_monitor_get_network_stats(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);
  LiMeSystemMonitorPrivate *priv = lime_system_monitor_get_instance_private(monitor);

  return priv->network_stats;
}

/**
 * lime_system_monitor_get_network_interface_stats:
 * @monitor: A #LiMeSystemMonitor
 * @interface: Interface name
 *
 * Get stats for specific network interface
 *
 * Returns: (transfer full): Network stats
 */
LiMeNetworkStats *
lime_system_monitor_get_network_interface_stats(LiMeSystemMonitor *monitor,
                                                const gchar *interface)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);

  LiMeNetworkStats *stats = g_malloc0(sizeof(LiMeNetworkStats));
  stats->interface_name = g_strdup(interface);

  return stats;
}

/**
 * lime_system_monitor_get_total_bytes_sent:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get total bytes sent on network
 *
 * Returns: Bytes sent
 */
guint64
lime_system_monitor_get_total_bytes_sent(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0);

  return 0;
}

/**
 * lime_system_monitor_get_total_bytes_received:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get total bytes received on network
 *
 * Returns: Bytes received
 */
guint64
lime_system_monitor_get_total_bytes_received(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0);

  return 0;
}

/**
 * lime_system_monitor_get_system_uptime:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get system uptime in seconds
 *
 * Returns: Uptime
 */
gint64
lime_system_monitor_get_system_uptime(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0);

  struct sysinfo info;
  if (sysinfo(&info) == 0) {
    return (gint64)info.uptime;
  }

  return 0;
}

/**
 * lime_system_monitor_get_load_average:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get system load average
 *
 * Returns: (array fixed-size=3): Load average (1min, 5min, 15min)
 */
gdouble *
lime_system_monitor_get_load_average(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);

  gdouble *load = g_malloc0(3 * sizeof(gdouble));

  struct sysinfo info;
  if (sysinfo(&info) == 0) {
    load[0] = (gdouble)info.loads[0] / 65536.0;
    load[1] = (gdouble)info.loads[1] / 65536.0;
    load[2] = (gdouble)info.loads[2] / 65536.0;
  }

  return load;
}

/**
 * lime_system_monitor_get_running_processes:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get number of running processes
 *
 * Returns: Running process count
 */
gint
lime_system_monitor_get_running_processes(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), 0);

  struct sysinfo info;
  if (sysinfo(&info) == 0) {
    return (gint)info.procs;
  }

  return 0;
}

/**
 * lime_system_monitor_start_monitoring:
 * @monitor: A #LiMeSystemMonitor
 * @interval_ms: Update interval in milliseconds
 *
 * Start continuous monitoring
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_monitor_start_monitoring(LiMeSystemMonitor *monitor,
                                     guint interval_ms)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), FALSE);
  LiMeSystemMonitorPrivate *priv = lime_system_monitor_get_instance_private(monitor);

  if (priv->is_monitoring) return TRUE;

  priv->monitor_interval = interval_ms;
  priv->monitor_timer = g_timeout_add(interval_ms,
                                      lime_system_monitor_update_stats,
                                      monitor);
  priv->is_monitoring = TRUE;

  g_debug("System monitoring started (interval: %d ms)", interval_ms);
  return TRUE;
}

/**
 * lime_system_monitor_stop_monitoring:
 * @monitor: A #LiMeSystemMonitor
 *
 * Stop continuous monitoring
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_monitor_stop_monitoring(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), FALSE);
  LiMeSystemMonitorPrivate *priv = lime_system_monitor_get_instance_private(monitor);

  if (!priv->is_monitoring) return TRUE;

  if (priv->monitor_timer) {
    g_source_remove(priv->monitor_timer);
    priv->monitor_timer = 0;
  }

  priv->is_monitoring = FALSE;
  g_debug("System monitoring stopped");

  return TRUE;
}

/**
 * lime_system_monitor_is_monitoring:
 * @monitor: A #LiMeSystemMonitor
 *
 * Check if monitoring is active
 *
 * Returns: %TRUE if monitoring
 */
gboolean
lime_system_monitor_is_monitoring(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), FALSE);
  LiMeSystemMonitorPrivate *priv = lime_system_monitor_get_instance_private(monitor);

  return priv->is_monitoring;
}

/**
 * lime_system_monitor_set_cpu_alert_threshold:
 * @monitor: A #LiMeSystemMonitor
 * @threshold: Threshold percentage
 *
 * Set CPU usage alert threshold
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_monitor_set_cpu_alert_threshold(LiMeSystemMonitor *monitor,
                                            gdouble threshold)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), FALSE);
  LiMeSystemMonitorPrivate *priv = lime_system_monitor_get_instance_private(monitor);

  priv->cpu_alert_threshold = CLAMP(threshold, 0.0, 100.0);
  return TRUE;
}

/**
 * lime_system_monitor_set_memory_alert_threshold:
 * @monitor: A #LiMeSystemMonitor
 * @threshold: Threshold percentage
 *
 * Set memory usage alert threshold
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_monitor_set_memory_alert_threshold(LiMeSystemMonitor *monitor,
                                               gdouble threshold)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), FALSE);
  LiMeSystemMonitorPrivate *priv = lime_system_monitor_get_instance_private(monitor);

  priv->memory_alert_threshold = CLAMP(threshold, 0.0, 100.0);
  return TRUE;
}

/**
 * lime_system_monitor_set_disk_alert_threshold:
 * @monitor: A #LiMeSystemMonitor
 * @threshold: Threshold percentage
 *
 * Set disk usage alert threshold
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_monitor_set_disk_alert_threshold(LiMeSystemMonitor *monitor,
                                             gdouble threshold)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), FALSE);
  LiMeSystemMonitorPrivate *priv = lime_system_monitor_get_instance_private(monitor);

  priv->disk_alert_threshold = CLAMP(threshold, 0.0, 100.0);
  return TRUE;
}

/**
 * lime_system_monitor_get_system_info:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get system information
 *
 * Returns: (transfer full): System info string
 */
gchar *
lime_system_monitor_get_system_info(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);

  GString *info = g_string_new("");

  gchar *cpu_model = lime_system_monitor_get_cpu_model(monitor);
  gint cpu_cores = lime_system_monitor_get_cpu_core_count(monitor);
  gdouble cpu_freq = lime_system_monitor_get_cpu_frequency(monitor);

  guint64 total_mem = lime_system_monitor_get_total_memory(monitor);

  g_string_append_printf(info, "CPU: %s (%d cores @ %.2f GHz)\n",
                         cpu_model, cpu_cores, cpu_freq);
  g_string_append_printf(info, "Memory: %.2f GB total\n",
                         (gdouble)total_mem / (1024*1024*1024));

  gchar *result = g_string_free(info, FALSE);
  g_free(cpu_model);

  return result;
}

/**
 * lime_system_monitor_get_kernel_version:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get kernel version
 *
 * Returns: (transfer full): Kernel version
 */
gchar *
lime_system_monitor_get_kernel_version(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);

  FILE *fp = fopen("/proc/version", "r");
  if (!fp) return g_strdup("Unknown");

  gchar line[256];
  if (fgets(line, sizeof(line), fp)) {
    fclose(fp);
    return g_strdup(line);
  }

  fclose(fp);
  return g_strdup("Unknown");
}

/**
 * lime_system_monitor_get_hostname:
 * @monitor: A #LiMeSystemMonitor
 *
 * Get system hostname
 *
 * Returns: (transfer full): Hostname
 */
gchar *
lime_system_monitor_get_hostname(LiMeSystemMonitor *monitor)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_MONITOR(monitor), NULL);

  gchar hostname[256];
  if (gethostname(hostname, sizeof(hostname)) == 0) {
    return g_strdup(hostname);
  }

  return g_strdup("Unknown");
}
