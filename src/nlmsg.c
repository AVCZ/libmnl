/*
 * (C) 2008-2010 by Pablo Neira Ayuso <pablo@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <libmnl/libmnl.h>
#include "internal.h"

/**
 * \defgroup nlmsg Netlink message helpers
 *
 * Netlink message:
 * \verbatim
	|<----------------- 4 bytes ------------------->|
	|<----- 2 bytes ------>|<------- 2 bytes ------>|
	|-----------------------------------------------|
	|      Message length (including header)        |
	|-----------------------------------------------|
	|     Message type     |     Message flags      |
	|-----------------------------------------------|
	|           Message sequence number             |
	|-----------------------------------------------|
	|                 Netlink PortID                |
	|-----------------------------------------------|
	|                                               |
	.                   Payload                     .
	|_______________________________________________|
\endverbatim
 *
 * There is usually an extra header after the the Netlink header (at the
 * beginning of the payload). This extra header is specific of the Netlink
 * subsystem. After this extra header, it comes the sequence of attributes
 * that are expressed in Type-Length-Value (TLV) format.
 *
 * @{
 */

/**
 * mnl_nlmsg_size - calculate the size of Netlink message (without alignment)
 * \param len length of the Netlink payload
 *
 * This function returns the size of a netlink message (header plus payload)
 * without alignment.
 */
EXPORT_SYMBOL size_t mnl_nlmsg_size(size_t len)
{
	return len + MNL_NLMSG_HDRLEN;
}

/**
 * mnl_nlmsg_aligned_size - calculate the aligned size of Netlink messages
 * \param len length of the Netlink payload
 *
 * This function returns the size of a netlink message (header plus payload)
 * with alignment.
 */
size_t mnl_nlmsg_aligned_size(size_t len)
{
	return MNL_ALIGN(mnl_nlmsg_size(len));
}

/**
 * mnl_nlmsg_get_payload_len - get the length of the Netlink payload
 * \param nlh pointer to the header of the Netlink message
 *
 * This function returns the Length of the netlink payload, ie. the length
 * of the full message minus the size of the Netlink header.
 */
EXPORT_SYMBOL size_t mnl_nlmsg_get_payload_len(const struct nlmsghdr *nlh)
{
	return nlh->nlmsg_len - MNL_NLMSG_HDRLEN;
}

/**
 * mnl_nlmsg_put_header - reserve and prepare room for Netlink header
 * \param buf memory already allocated to store the Netlink header
 *
 * This function sets to zero the room that is required to put the Netlink
 * header in the memory buffer passed as parameter. This function also
 * initializes the nlmsg_len field to the size of the Netlink header. This
 * function returns a pointer to the Netlink header structure.
 */
EXPORT_SYMBOL struct nlmsghdr *mnl_nlmsg_put_header(void *buf)
{
	int len = MNL_ALIGN(sizeof(struct nlmsghdr));
	struct nlmsghdr *nlh = buf;

	memset(buf, 0, len);
	nlh->nlmsg_len = len;
	return nlh;
}

/**
 * mnl_nlmsg_put_extra_header - reserve and prepare room for an extra header
 * \param nlh pointer to Netlink header
 * \param size size of the extra header that we want to put
 *
 * This function sets to zero the room that is required to put the extra
 * header after the initial Netlink header. This function also increases
 * the nlmsg_len field. You have to invoke mnl_nlmsg_put_header() before
 * you call this function. This function returns a pointer to the extra
 * header.
 */
EXPORT_SYMBOL void *
mnl_nlmsg_put_extra_header(struct nlmsghdr *nlh, size_t size)
{
	char *ptr = (char *)nlh + nlh->nlmsg_len;
	nlh->nlmsg_len += MNL_ALIGN(size);
	memset(ptr, 0, size);
	return ptr;
}

/**
 * mnl_nlmsg_get_payload - get a pointer to the payload of the netlink message
 * \param nlh pointer to a netlink header
 *
 * This function returns a pointer to the payload of the netlink message.
 */
EXPORT_SYMBOL void *mnl_nlmsg_get_payload(const struct nlmsghdr *nlh)
{
	return (void *)nlh + MNL_NLMSG_HDRLEN;
}

/**
 * mnl_nlmsg_get_payload_offset - get a pointer to the payload of the message
 * \param nlh pointer to a netlink header
 * \param offset offset to the payload of the attributes TLV set
 *
 * This function returns a pointer to the payload of the netlink message plus
 * a given offset.
 */
EXPORT_SYMBOL void *
mnl_nlmsg_get_payload_offset(const struct nlmsghdr *nlh, size_t offset)
{
	return (void *)nlh + MNL_NLMSG_HDRLEN + MNL_ALIGN(offset);
}

/**
 * mnl_nlmsg_ok - check a there is room for netlink message
 * \param nlh netlink message that we want to check
 * \param len remaining bytes in a buffer that contains the netlink message
 *
 * This function is used to check that a buffer that contains a netlink
 * message has enough room for the netlink message that it stores, ie. this
 * function can be used to verify that a netlink message is not malformed nor
 * truncated.
 *
 * This function does not set errno in case of error since it is intended
 * for iterations. Thus, it returns 1 on success and 0 on error.
 *
 * The len parameter may become negative in malformed messages during message
 * iteration, that is why we use a signed integer.
 */
EXPORT_SYMBOL bool mnl_nlmsg_ok(const struct nlmsghdr *nlh, int len)
{
	return len >= (int)sizeof(struct nlmsghdr) &&
	       nlh->nlmsg_len >= sizeof(struct nlmsghdr) &&
	       (int)nlh->nlmsg_len <= len;
}

/**
 * mnl_nlmsg_next - get the next netlink message in a multipart message
 * \param nlh current netlink message that we are handling
 * \param len length of the remaining bytes in the buffer (passed by reference).
 *
 * This function returns a pointer to the next netlink message that is part
 * of a multi-part netlink message. Netlink can batch several messages into
 * one buffer so that the receiver has to iterate over the whole set of
 * Netlink messages.
 *
 * You have to use mnl_nlmsg_ok() to check if the next Netlink message is
 * valid.
 */
EXPORT_SYMBOL struct nlmsghdr *
mnl_nlmsg_next(const struct nlmsghdr *nlh, int *len)
{
	*len -= MNL_ALIGN(nlh->nlmsg_len);
	return (struct nlmsghdr *)((void *)nlh + MNL_ALIGN(nlh->nlmsg_len));
}

/**
 * mnl_nlmsg_get_payload_tail - get the ending of the netlink message
 * \param nlh pointer to netlink message
 *
 * This function returns a pointer to the netlink message tail. This is useful
 * to build a message since we continue adding attributes at the end of the
 * message.
 */
EXPORT_SYMBOL void *mnl_nlmsg_get_payload_tail(const struct nlmsghdr *nlh)
{
	return (void *)nlh + MNL_ALIGN(nlh->nlmsg_len);
}

/**
 * mnl_nlmsg_seq_ok - perform sequence tracking
 * \param nlh current netlink message that we are handling
 * \param seq last sequence number used to send a message
 *
 * This functions returns true if the sequence tracking is fulfilled, otherwise
 * false is returned. We skip the tracking for netlink messages whose sequence
 * number is zero since it is usually reserved for event-based kernel
 * notifications. On the other hand, if seq is set but the message sequence
 * number is not set (i.e. this is an event message coming from kernel-space),
 * then we also skip the tracking. This approach is good if we use the same
 * socket to send commands to kernel-space (that we want to track) and to
 * listen to events (that we do not track).
 */
EXPORT_SYMBOL bool
mnl_nlmsg_seq_ok(const struct nlmsghdr *nlh, unsigned int seq)
{
	return nlh->nlmsg_seq && seq ? nlh->nlmsg_seq == seq : true;
}

/**
 * mnl_nlmsg_portid_ok - perform portID origin check
 * \param nlh current netlink message that we are handling
 * \param seq netlink portid that we want to check
 *
 * This functions returns true if the origin is fulfilled, otherwise
 * false is returned. We skip the tracking for netlink message whose portID
 * is zero since it is reserved for event-based kernel notifications. On the
 * other hand, if portid is set but the message PortID is not (i.e. this
 * is an event message coming from kernel-space), then we also skip the
 * tracking. This approach is good if we use the same socket to send commands
 * to kernel-space (that we want to track) and to listen to events (that we
 * do not track).
 */
EXPORT_SYMBOL bool
mnl_nlmsg_portid_ok(const struct nlmsghdr *nlh, unsigned int portid)
{
	return nlh->nlmsg_pid && portid ? nlh->nlmsg_pid == portid : true;
}

static void mnl_nlmsg_fprintf_header(FILE *fd, const struct nlmsghdr *nlh)
{
	fprintf(fd, "----------------\t------------------\n");
	fprintf(fd, "|  %.010u  |\t| message length |\n", nlh->nlmsg_len);
	fprintf(fd, "| %.05u | %c%c%c%c |\t|  type | flags  |\n",
		nlh->nlmsg_type,
		nlh->nlmsg_flags & NLM_F_REQUEST ? 'R' : '-',
		nlh->nlmsg_flags & NLM_F_MULTI ? 'M' : '-',
		nlh->nlmsg_flags & NLM_F_ACK ? 'A' : '-',
		nlh->nlmsg_flags & NLM_F_ECHO ? 'E' : '-');
	fprintf(fd, "|  %.010u  |\t| sequence number|\n", nlh->nlmsg_seq);
	fprintf(fd, "|  %.010u  |\t|     port ID    |\n", nlh->nlmsg_pid);
	fprintf(fd, "----------------\t------------------\n");
}

static void
mnl_nlmsg_fprintf_payload(FILE *fd, const struct nlmsghdr *nlh,
			  size_t extra_header_size)
{
	int rem = 0;
	unsigned int i;

	for (i=sizeof(struct nlmsghdr); i<nlh->nlmsg_len; i+=4) {
		char *b = (char *) nlh;
		struct nlattr *attr = (struct nlattr *) (b+i);

		/* netlink control message. */
		if (nlh->nlmsg_type < NLMSG_MIN_TYPE) {
			fprintf(fd, "| %.2x %.2x %.2x %.2x  |\t",
				0xff & b[i],	0xff & b[i+1],
				0xff & b[i+2],	0xff & b[i+3]);
			fprintf(fd, "|                |\n");
		/* special handling for the extra header. */
		} else if (extra_header_size > 0) {
			extra_header_size -= 4;
			fprintf(fd, "| %.2x %.2x %.2x %.2x  |\t",
				0xff & b[i],	0xff & b[i+1],
				0xff & b[i+2],	0xff & b[i+3]);
			fprintf(fd, "|  extra header  |\n");
		/* this seems like an attribute header. */
		} else if (rem == 0 && (attr->nla_type & NLA_TYPE_MASK) != 0) {
			fprintf(fd, "|%c[%d;%dm"
				    "%.5u"
				    "%c[%dm"
				    "|"
				    "%c[%d;%dm"
				    "%c%c"
				    "%c[%dm"
				    "|"
				    "%c[%d;%dm"
				    "%.5u"
				    "%c[%dm|\t",
				27, 1, 31,
				attr->nla_len,
				27, 0,
				27, 1, 32,
				attr->nla_type & NLA_F_NESTED ? 'N' : '-',
				attr->nla_type &
					NLA_F_NET_BYTEORDER ? 'B' : '-',
				27, 0,
				27, 1, 34,
				attr->nla_type & NLA_TYPE_MASK,
				27, 0);
			fprintf(fd, "|len |flags| type|\n");

			if (!(attr->nla_type & NLA_F_NESTED)) {
				rem = NLA_ALIGN(attr->nla_len) -
					sizeof(struct nlattr);
			}
		/* this is the attribute payload. */
		} else if (rem > 0) {
			rem -= 4;
			fprintf(fd, "| %.2x %.2x %.2x %.2x  |\t",
				0xff & b[i],	0xff & b[i+1],
				0xff & b[i+2],	0xff & b[i+3]);
			fprintf(fd, "|      data      |");
			fprintf(fd, "\t %c %c %c %c\n",
				isalnum(b[i]) ? b[i] : 0,
				isalnum(b[i+1]) ? b[i+1] : 0,
				isalnum(b[i+2]) ? b[i+2] : 0,
				isalnum(b[i+3]) ? b[i+3] : 0);
		}
	}
	fprintf(fd, "----------------\t------------------\n");
}

/**
 * mnl_nlmsg_fprintf - print netlink message to file
 * \param fd pointer to file type
 * \param data pointer to the buffer that contains messages to be printed
 * \param datalen length of data stored in the buffer
 * \param extra_header_size size of the extra header (if any)
 *
 * This function prints the netlink header to a file handle.
 * It may be useful for debugging purposes. One example of the output
 * is the following:
 *
 *\verbatim
----------------        ------------------
|  0000000040  |        | message length |
| 00016 | R-A- |        |  type | flags  |
|  1289148991  |        | sequence number|
|  0000000000  |        |     port ID    |
----------------        ------------------
| 00 00 00 00  |        |  extra header  |
| 00 00 00 00  |        |  extra header  |
| 01 00 00 00  |        |  extra header  |
| 01 00 00 00  |        |  extra header  |
|00008|--|00003|        |len |flags| type|
| 65 74 68 30  |        |      data      |       e t h 0
----------------        ------------------
\endverbatim
 *
 * This example above shows the netlink message that is send to kernel-space
 * to set up the link interface eth0. The netlink and attribute header data
 * are displayed in base 10 whereas the extra header and the attribute payload
 * are expressed in base 16. The possible flags in the netlink header are:
 *
 * - R, that indicates that NLM_F_REQUEST is set.
 * - M, that indicates that NLM_F_MULTI is set.
 * - A, that indicates that NLM_F_ACK is set.
 * - E, that indicates that NLM_F_ECHO is set.
 *
 * The lack of one flag is displayed with '-'. On the other hand, the possible
 * attribute flags available are:
 *
 * - N, that indicates that NLA_F_NESTED is set.
 * - B, that indicates that NLA_F_NET_BYTEORDER is set.
 */
EXPORT_SYMBOL void
mnl_nlmsg_fprintf(FILE *fd, const void *data, size_t datalen,
		  size_t extra_header_size)
{
	const struct nlmsghdr *nlh = data;
	int len = datalen;

	while (mnl_nlmsg_ok(nlh, len)) {
		mnl_nlmsg_fprintf_header(fd, nlh);
		mnl_nlmsg_fprintf_payload(fd, nlh, extra_header_size);
		nlh = mnl_nlmsg_next(nlh, &len);
	}
}

/**
 * @}
 */
