// gcc lgetattr.c; ./a.out

#include <stdio.h>
       #include <sys/types.h>
       #include <attr/xattr.h>

// lgetxattr("/var/lib/samba/usershares/gdrive", "security.selinux", 0xc11ea0, 255) = -1 EPERM (Operation not permitted)

void main() {
	char *p = "/var/lib/samba/usershares/gdrive/";
	char v[256];
	int rv = lgetxattr(p, "security.selinux", &v, sizeof(v) - 1);
	if (rv == -1) {
		printf("fail %d\n", errno);
		perror("Says:");
	} else {
		printf("ok\n");
	}
	return;
}
