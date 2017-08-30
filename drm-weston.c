#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>

#define WESTON_LAUNCHER_OPEN 0

#define WESTON_LAUNCHER_SUCCESS    0
#define WESTON_LAUNCHER_ACTIVATE   1
#define WESTON_LAUNCHER_DEACTIVATE 2

struct weston_launcher_message {
	int opcode;
};

struct weston_launcher_open {
	struct weston_launcher_message header;
	int flags;
	char path[];
};

static int send_open(int sockfd, const char* device, int mode)
{
	struct weston_launcher_open* req;
	int devlen = strlen(device);
	int msglen = sizeof(*req) + devlen + 1;
	char message[msglen];

	req = (struct weston_launcher_open*) message;
	req->header.opcode = WESTON_LAUNCHER_OPEN;
	req->flags = mode;
	memcpy(req->path, device, devlen + 1);

	if(send(sockfd, message, msglen, 0) < 0) {
		printf("weston-launch send: %m\n");
		return -1;
	}

	return 0;
}

static int fetch_cmsg_fd(struct cmsghdr* cmsg)
{
	if(!cmsg) {
		printf("weston-launch: no cmsg\n");
		return -1;
	} if(cmsg->cmsg_level != SOL_SOCKET) {
		printf("weston-launch: not SOL_SOCKET\n");
		return -1;
	} if(cmsg->cmsg_type != SCM_RIGHTS) {
		printf("weston-launch: not SCM_RIGHTS\n");
		return -1;
	}

	union fdmsg {
		char b[4];
		int fd;
	} *data = (void*)CMSG_DATA(cmsg);

	return data->fd;
}

static int recv_open(int sockfd, const char* device)
{
	char repbuf[16];
	char control[32];
	struct iovec iov = {
		.iov_base = repbuf,
		.iov_len = sizeof(repbuf)
	};
	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = control,
		.msg_controllen = sizeof(control),
		.msg_flags = 0
	};

	struct weston_launcher_message* rep;
	int ret;

	if((ret = recvmsg(sockfd, &msg, MSG_CMSG_CLOEXEC)) < 0) {
		printf("weston-launch recv: %m\n");
		return ret;
	}

	if(ret != sizeof(*rep)) {
		printf("weston-launch recv: invalid reply\n");
		return -1;
	}

	rep = (struct weston_launcher_message*) repbuf;

	if(rep->opcode < 0) {
		printf("weston-lauch open %s: %s\n", device,
				strerror(rep->opcode));
		return -1;
	}

	if((ret = fetch_cmsg_fd(CMSG_FIRSTHDR(&msg))) < 0) {
		printf("weston-launch invalid control message\n");
		return -1;
	}

	return ret;
}

int open_weston(const char* wlsock, const char* device, int mode)
{
	int fd, ret;

	if((fd = atoi(wlsock)) < 0) {
		printf("invalid weston-launch socket fd\n");
		return fd;
	}

	if((ret = send_open(fd, device, mode)) < 0)
		return ret;

	if((ret = recv_open(fd, device)) < 0)
		return ret;

	return ret;
}
