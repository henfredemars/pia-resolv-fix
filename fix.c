
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <dirent.h>
#include <syslog.h>
#include <errno.h>

#define RESOLV_SYM_PATH "/run/resolvconf/resolv.conf"
#define RESOLV_PATH "/etc/resolv.conf"
#define RESOLV_PPATH "/etc"
#define EVENT_QUEUE_SIZE 4
#define MAX_EVENT_SIZE (sizeof(struct inotify_event)+NAME_MAX+1)

void* checked_malloc(size_t bytes)
{
  void* m = malloc(bytes);
  if (!m) {
    perror("Malloc failed");
    abort();
  }
  return m;
}

int make_pid_file(const char* path)
{
  FILE* fd = fopen(path, "w");
  if (fd) {
    fprintf(fd, "%ld", (long)getpid());
    fclose(fd);
    return 0;
  }
  perror("Failed to write pid file");
  return -1;
}

int do_fork()
{
  const pid_t fpid = fork();
  if (fpid < 0) {
    perror("Failed to fork()");
    return -1;
  } else if (fpid > 0) {
    // parent, you can exit
    exit(0);
  }

  return 0;
}

void daemonize()
{
  do_fork();
  if (setsid() < 0)
    perror("Failed to setsid()");

  // old tyme double-fork magic
  do_fork();

  if (chdir("/"))
    perror("Failed to free working directory");
  close(STDIN_FILENO);
}

pid_t proc_find_cmdline(const char* phrase)
{
  DIR* dir;
  struct dirent* ent;
  char buf[1024];

  // open /proc
  dir = opendir("/proc");
  if (!dir) {
    syslog(LOG_ERR, "Error opening /proc");
    return -1;
  }

  // look for pid
  while ((ent = readdir(dir)) != NULL) {
    char* endptr;
    long lpid = strtol(ent->d_name, &endptr, 10);
    if (*endptr != '\0')
      // not a pid
      continue;

    // try to open cmdline
    snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", lpid);
    FILE* fp = fopen(buf, "r");
    if (!fp)
      continue;
    if (fgets(buf, sizeof(buf), fp) != NULL) {
      if (strstr(buf, phrase) != NULL) {
        fclose(fp);
        closedir(dir);
        return (pid_t)lpid;
      }
    }

    fclose(fp);

  }

  // cleanup
  closedir(dir);
  return -1;
}


int pia_running()
{
  return proc_find_cmdline("openvpn") >= 0;
}

void handle_arguments(int argc, char** argv)
{
  int c;
  int want_daemonize = 0;
  int want_pid_file = 0;
  char pid_buf[1024];

  while ((c = getopt(argc, argv, "dp:")) > 0) {
    if (c == 'd') {
      want_daemonize = 1;
    } else if (c == 'p') {
      if (snprintf(pid_buf, sizeof(pid_buf), "%s", optarg))
        want_pid_file = 1;
    } else if (c == '?') {
      if (optopt == 'p')
        fprintf(stderr, "Option 'p' requires an argument\n");
      else
        fprintf(stderr, "Unknown option '%c'\n", optopt);
      exit(1);
    } else {
      fprintf(stderr, "Unexpected getopt() result shouldn't happen\n");
      exit(1);
    }
  }

  if (want_daemonize)
    daemonize();
  if (want_pid_file)
    make_pid_file(pid_buf);

  syslog(LOG_INFO, "Parsed arguments: daemon=%d, pid_file=%d", want_daemonize,
    want_pid_file);
}

void fix_symlink()
{
  syslog(LOG_INFO, "Restoring symlink to %s", RESOLV_SYM_PATH);
  if (remove(RESOLV_PATH))
    if (errno != ENOENT)
      syslog(LOG_ERR, "Failed to remove existing file resolv.conf");
  if (symlink(RESOLV_SYM_PATH, RESOLV_PATH))
    syslog(LOG_ERR, "Failed to create symlink to dynamic resolv.conf");
}

void check_resolv()
{
  struct stat r_stat;
  const int pia = pia_running();

  if (lstat(RESOLV_PATH, &r_stat)) {
    if (!pia) {
      syslog(LOG_INFO,
        "Failed to stat. Trying to fix because PIA is not running right now...");
      fix_symlink();
    } else {
      syslog(LOG_INFO, "Cannot stat, assuming PIA must be writing resolv.conf");
    }
  } else {
    if (S_ISLNK(r_stat.st_mode))
      if (pia)
        syslog(LOG_NOTICE, "Bug? Symlink exists but PIA is running. Weird...");
      else
        syslog(LOG_INFO, "All good, symlink and PIA isn't running");
    else
      if (pia)
        syslog(LOG_INFO, "All good, not a symlink and PIA is running");
      else
        fix_symlink();
  }
}

void watch_resolv()
{
  int fd;
  if ((fd = inotify_init()) < 0) {
    syslog(LOG_ERR, "Failed to create inotify instance");
    exit(1);
  }

  int wd;
  if ((wd = inotify_add_watch(fd, RESOLV_PPATH, IN_CREATE | IN_DELETE)) < 0) {
    syslog(LOG_ERR, "Failed to inotify_add_watch");
    close(fd);
    exit(1);
  }

  const size_t buf_size = EVENT_QUEUE_SIZE*MAX_EVENT_SIZE;
  const size_t header_size = sizeof(struct inotify_event);
  union {
    struct inotify_event e;
    char padding[MAX_EVENT_SIZE];
  } padded_event;
  syslog(LOG_INFO, "Allocating event buffer of %ld bytes", (long)buf_size);
  char* event_buf = checked_malloc(buf_size);

  syslog(LOG_INFO, "Begun monitoring %s/resolv.conf for changes", RESOLV_PPATH);
  size_t bytes_read;
  while ((bytes_read = read(fd, event_buf, buf_size)) > 0) {
    if (bytes_read < header_size) break;
    size_t event_start = 0;
    while (event_start < bytes_read) {
      memcpy(&padded_event, event_buf + event_start, header_size);
      uint32_t len = padded_event.e.len;
      memcpy(&padded_event+header_size, event_buf+event_start+header_size, len);
      if (padded_event.e.mask & IN_IGNORED) {
        syslog(LOG_ERR, "Kernel removed our watch! Terminating...");
        exit(1);
      } else if (padded_event.e.mask & IN_Q_OVERFLOW) {
        syslog(LOG_WARNING, "Kernel event queue overflow, forcing check");
        check_resolv();
      } else if (strcmp(padded_event.e.name, "resolv.conf") == 0) {
        syslog(LOG_INFO, "Check resolv status prompted by inotify");
        check_resolv();
      }
      event_start += header_size + len;
    }
  }

  fprintf(stderr, "Got unexpected %ld on inotify read\n", (long)bytes_read);
  free(event_buf);
  close(fd);

  return;
}

int main(int argc, char** argv)
{
  openlog("pia-resolv-fix", LOG_PID, LOG_DAEMON);
  syslog(LOG_INFO, "Launching pia-resolv-fix");
  handle_arguments(argc, argv);
  check_resolv();
  watch_resolv();

  return 0;

}
