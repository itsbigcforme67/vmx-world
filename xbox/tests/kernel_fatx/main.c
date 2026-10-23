// Mounts the hard disk's partition 3 (X:), which tools/kernel_fatx_test.sh filled with a FATX volume made by
// vmx-firmware/core/fatx.c (16-bit cluster table, like a memory unit's), and has the Xbox kernel read and write it:
// lists every folder, reads every file (sizes and a checksum to the serial port), then writes a file, a folder with a
// file in it, replaces one file and deletes another, for the PC tool to read back. Serial: COM1 (xemu -serial file).
#include <hal/xbox.h>
#include <nxdk/mount.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
static unsigned char inb(unsigned short p) { unsigned char v; __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static void outb(unsigned short p, unsigned char v) { __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p)); }
static void ser(const char *s) { while (*s) { while (!(inb(0x3FD) & 0x20)) {} outb(0x3F8, (unsigned char)*s++); } }
static char line[300];
static unsigned sum(const char *path, long *n) {
  FILE *f = fopen(path, "rb"); unsigned s = 0; *n = -1; if (!f) return 0;
  int c; *n = 0; while ((c = fgetc(f)) != EOF) { s = s * 31 + (unsigned)c; (*n)++; }
  fclose(f); return s;
}
static void walk(const char *dir, int depth) {
  char pat[260]; snprintf(pat, sizeof pat, "%s*", dir);
  WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd); if (h == INVALID_HANDLE_VALUE) return;
  do {
    char p[260]; snprintf(p, sizeof p, "%s%s", dir, fd.cFileName);
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { snprintf(line, sizeof line, "D %s\n", p); ser(line); strcat(p, "\\"); if (depth < 8) walk(p, depth + 1); }
    else { long n; unsigned s = sum(p, &n); snprintf(line, sizeof line, "F %s %ld %08x\n", p, n, s); ser(line); }
  } while (FindNextFileA(h, &fd));
  FindClose(h);
}
int main(void) {
  ser("KFATX: start\n");
  if (!nxMountDrive('X', "\\Device\\Harddisk0\\Partition3\\")) { ser("KFATX: mount failed\n"); goto out; }
  walk("X:\\", 0);
  FILE *f = fopen("X:\\kernel.txt", "wb"); if (f) { fputs("written by the Xbox kernel\r\n", f); fclose(f); ser("KFATX: wrote kernel.txt\n"); }
  CreateDirectoryA("X:\\kdir", NULL);
  f = fopen("X:\\kdir\\inner.bin", "wb"); if (f) { for (int i = 0; i < 40000; i++) fputc(i * 7, f); fclose(f); ser("KFATX: wrote kdir\\inner.bin\n"); }
  f = fopen("X:\\replace.bin", "wb"); if (f) { fputs("replaced by the kernel", f); fclose(f); ser("KFATX: replaced replace.bin\n"); }
  if (DeleteFileA("X:\\delete.me")) ser("KFATX: deleted delete.me\n"); else ser("KFATX: delete failed\n");
  ser("KFATX: done\n");
out:
  for (;;) Sleep(1000);
}
