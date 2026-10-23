/* Copied from the mh1-xbox repo (src/pc/xbox/xbox_path.h, same owner) for FOLD's network loader. */
/* xbox_path.h: the folder the running XBE was started from, as a DOS path ("E:\Games\MHG", no
 * trailing backslash), and its file name ("mhg.xbe"). Shared by the network loader (which game it
 * serves, src/pc/xbox/loader/main.c) and the game (its log file and the loader it returns to,
 * xbox_log.c). Only the hard disk's E: partition (\Device\Harddisk0\Partition1) is mapped; anything
 * else (xemu's DVD, another partition) returns 0 and the caller uses its default folder. */
#ifndef XBOX_PATH_H
#define XBOX_PATH_H
#include <xboxkrnl/xboxkrnl.h>
#include <stdio.h>
#include <string.h>

static int xbox_image_dir(char *dir, size_t ndir, char *name, size_t nname)
{
    static const char pre[] = "\\Device\\Harddisk0\\Partition1\\";
    size_t n = XeImageFileName->Length, k = sizeof pre - 1, i;
    char p[260], *slash;
    if (n <= k || n >= sizeof p)
        return 0;
    memcpy(p, XeImageFileName->Buffer, n);
    p[n] = 0;
    for (i = 0; i < k; i++)
        if ((p[i] | 0x20) != (pre[i] | 0x20))
            return 0;
    if ((slash = strrchr(p, '\\')) == NULL || slash < p + k)
        return 0;
    *slash = 0;
    if (dir)
        snprintf(dir, ndir, "E:\\%s", p + k);
    if (name)
        snprintf(name, nname, "%s", slash + 1);
    return 1;
}

/* "default.xbe" in any case (the dashboards list a folder by that file) */
static int xbox_is_default_xbe(const char *name)
{
    static const char d[] = "default.xbe";
    size_t i;
    for (i = 0; d[i]; i++)
        if ((name[i] | 0x20) != d[i])
            return 0;
    return !name[i];
}
#endif
