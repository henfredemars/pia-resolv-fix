
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define RESOLV_PATH "/etc/resolv.conf"
#define RESOLV_SYM_PATH "/run/resolvconf/resolv.conf"

pid_t proc_find_cmdline(const char* phrase)
{
  DIR* dir;
  struct dirent* ent;
  char buf[1024];

  // open /proc
  dir = opendir("/proc");
  if (!dir) {
    perror("Error opening /proc");
    return 1;
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


int openvpn_running()
{
  return proc_find_cmdline("openvpn") >= 0;
}

int main()
{
  struct stat r_stat;

  // stat resolv.conf
  if (lstat(RESOLV_PATH, &r_stat)) {
    perror("Failed to stat resolv.conf");
    return 1;
  }

  if (!S_ISLNK(r_stat.st_mode)) {
    if (!openvpn_running()) {
      // assume leftover resolv.conf
      if (remove(RESOLV_PATH)) {
        perror("Failed to remove real file resolv.conf");
        return 2;
      }
      if (symlink(RESOLV_SYM_PATH, RESOLV_PATH)) {
        perror("Failed to create symlink to dynamic resolv.conf");
        return 3;
      }
    }
  }

  return 0;

}
