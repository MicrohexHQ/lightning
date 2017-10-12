#ifndef LIGHTNING_LIGHTNINGD_SUBD_H
#define LIGHTNING_LIGHTNINGD_SUBD_H
#include "config.h"
#include <ccan/endian/endian.h>
#include <ccan/list/list.h>
#include <ccan/short_types/short_types.h>
#include <ccan/tal/tal.h>
#include <common/msg_queue.h>

struct io_conn;

/* By convention, replies are requests + 100 */
#define SUBD_REPLY_OFFSET 100
/* And reply failures are requests + 200 */
#define SUBD_REPLYFAIL_OFFSET 200

/* One of our subds. */
struct subd {
	/* Name, like John, or "lightning_hsmd" */
	const char *name;
	/* The Big Cheese. */
	struct lightningd *ld;
	/* pid, for waiting for status when it dies. */
	int pid;
	/* Connection. */
	struct io_conn *conn;

	/* If we are associated with a single peer, this points to it. */
	struct peer *peer;

	/* For logging */
	struct log *log;

	/* Callback when non-reply message comes in. */
	int (*msgcb)(struct subd *, const u8 *, const int *);
	const char *(*msgname)(int msgtype);

	/* Buffer for input. */
	u8 *msg_in;

	/* While we're reading fds in. */
	size_t num_fds_in_read;
	int *fds_in;

	/* For global daemons: we fail if they fail. */
	bool must_not_exit;

	/* Messages queue up here. */
	struct msg_queue outq;

	/* Callbacks for replies. */
	struct list_head reqs;
};

/**
 * new_global_subd - create a new global subdaemon.
 * @ld: global state
 * @name: basename of daemon
 * @msgname: function to get name from messages
 * @msgcb: function to call when non-fatal message received (or NULL)
 * @...: NULL-terminated list of pointers to  fds to hand as fd 3, 4...
 *	(can be take, if so, set to -1)
 *
 * @msgcb gets called with @fds set to NULL: if it returns a positive number,
 * that many @fds are received before calling again.  If it returns -1, the
 * subdaemon is shutdown.
 */
struct subd *new_global_subd(struct lightningd *ld,
			     const char *name,
			     const char *(*msgname)(int msgtype),
			     int (*msgcb)(struct subd *, const u8 *,
					  const int *fds),
			     ...);

/**
 * new_peer_subd - create a new subdaemon for a specific peer.
 * @ld: global state
 * @name: basename of daemon
 * @peer: peer to associate.
 * @msgname: function to get name from messages
 * @msgcb: function to call when non-fatal message received (or NULL)
 * @...: NULL-terminated list of pointers to  fds to hand as fd 3, 4...
 *	(can be take, if so, set to -1)
 *
 * @msgcb gets called with @fds set to NULL: if it returns a positive number,
 * that many @fds are received before calling again.  If it returns -1, the
 * subdaemon is shutdown.
 */
struct subd *new_peer_subd(struct lightningd *ld,
			   const char *name,
			   struct peer *peer,
			   const char *(*msgname)(int msgtype),
			   int (*msgcb)(struct subd *, const u8 *,
					const int *fds),
			   ...);

/**
 * subd_raw - raw interface to get a subdaemon on an fd (for HSM)
 * @ld: global state
 * @name: basename of daemon
 */
int subd_raw(struct lightningd *ld, const char *name);

/**
 * subd_send_msg - queue a message to the subdaemon.
 * @sd: subdaemon to request
 * @msg_out: message (can be take)
 */
void subd_send_msg(struct subd *sd, const u8 *msg_out);

/**
 * subd_send_fd - queue a file descriptor to pass to the subdaemon.
 * @sd: subdaemon to request
 * @fd: the file descriptor (closed after passing).
 */
void subd_send_fd(struct subd *sd, int fd);

/**
 * subd_req - queue a request to the subdaemon.
 * @ctx: lifetime for the callback: if this is freed, don't call replycb.
 * @sd: subdaemon to request
 * @msg_out: request message (can be take)
 * @fd_out: if >=0 fd to pass at the end of the message (closed after)
 * @num_fds_in: how many fds to read in to hand to @replycb if it's a reply.
 * @replycb: callback when reply comes in, returns false to shutdown daemon.
 * @replycb_data: final arg to hand to @replycb
 *
 * @replycb cannot free @sd, so it returns false to remove it.
 * Note that @replycb is called for replies of type @msg_out + SUBD_REPLY_OFFSET
 * with @num_fds_in fds, or type @msg_out + SUBD_REPLYFAIL_OFFSET with no fds.
 */
#define subd_req(ctx, sd, msg_out, fd_out, num_fds_in, replycb, replycb_data) \
	subd_req_((ctx), (sd), (msg_out), (fd_out), (num_fds_in),	\
		  typesafe_cb_preargs(bool, void *,			\
				      (replycb), (replycb_data),	\
				      struct subd *,			\
				      const u8 *, const int *),		\
		       (replycb_data))
void subd_req_(const tal_t *ctx,
	       struct subd *sd,
	       const u8 *msg_out,
	       int fd_out, size_t num_fds_in,
	       bool (*replycb)(struct subd *, const u8 *, const int *, void *),
	       void *replycb_data);

/**
 * subd_release_peer - try to politely shut down a subdaemon.
 * @owner: subd which owned peer.
 * @peer: peer to release.
 *
 * If the subdaemon is not already shutting down, and it is a per-peer
 * subdaemon, this shuts it down.
 */
void subd_release_peer(struct subd *owner, struct peer *peer);

/**
 * subd_shutdown - try to politely shut down a subdaemon.
 * @subd: subd to shutdown.
 * @seconds: maximum seconds to wait for it to exit.
 *
 * This closes the fd to the subdaemon, and gives it a little while to exit.
 * The @finished callback will never be called.
 */
void subd_shutdown(struct subd *subd, unsigned int seconds);

char *opt_subd_debug(const char *optarg, struct lightningd *ld);
char *opt_subd_dev_disconnect(const char *optarg, struct lightningd *ld);

bool dev_disconnect_permanent(struct lightningd *ld);
#endif /* LIGHTNING_LIGHTNINGD_SUBD_H */
