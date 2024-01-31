// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2022 MediaTek Inc.
 */


/*
 * The following source code are based on the implementation of libfdt
 */

#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/kernel.h>

#include "mtk_pqu_fdt.h"


struct fdt_reserve_entry {
	fdt64_t address;
	fdt64_t size;
};

struct fdt_node_header {
	fdt32_t tag;
	char name[0];
};

struct fdt_property {
	fdt32_t tag;
	fdt32_t len;
	fdt32_t nameoff;
	char data[0];
};


/**********************************************************************/
/* General functions                                                  */
/**********************************************************************/
#define fdt_set_hdr_(name) \
	static inline void fdt_set_##name(void *fdt, uint32_t val) \
	{ \
		struct fdt_header *fdth = (struct fdt_header *)fdt; \
		fdth->name = cpu_to_fdt32(val); \
	}
fdt_set_hdr_(magic);
fdt_set_hdr_(totalsize);
fdt_set_hdr_(off_dt_struct);
fdt_set_hdr_(off_dt_strings);
fdt_set_hdr_(off_mem_rsvmap);
fdt_set_hdr_(version);
fdt_set_hdr_(last_comp_version);
fdt_set_hdr_(boot_cpuid_phys);
fdt_set_hdr_(size_dt_strings);
fdt_set_hdr_(size_dt_struct);
#undef fdt_set_hdr_

#define FDT_ALIGN(x, a)				(((x) + (a) - 1) & ~((a) - 1))
#define FDT_TAGALIGN(x)				(FDT_ALIGN((x), FDT_TAGSIZE))

int fdt_check_header(const void *fdt)
{
	if (fdt_magic(fdt) == FDT_MAGIC) {
		/* Complete tree */
		if (fdt_version(fdt) < FDT_FIRST_SUPPORTED_VERSION)
			return -FDT_ERR_BADVERSION;
		if (fdt_last_comp_version(fdt) > FDT_LAST_SUPPORTED_VERSION)
			return -FDT_ERR_BADVERSION;
	} else if (fdt_magic(fdt) == FDT_SW_MAGIC) {
		/* Unfinished sequential-write blob */
		if (fdt_size_dt_struct(fdt) == 0)
			return -FDT_ERR_BADSTATE;
	} else {
		return -FDT_ERR_BADMAGIC;
	}

	return 0;
}


/**********************************************************************/
/* Traversal functions                                                */
/**********************************************************************/
static inline const void *fdt_offset_ptr_(const void *fdt, int offset)
{
	return (const char *)fdt + fdt_off_dt_struct(fdt) + offset;
}

static inline void *fdt_offset_ptr_w_(void *fdt, int offset)
{
	return (void *)(uintptr_t)fdt_offset_ptr_(fdt, offset);
}

static inline const struct fdt_reserve_entry *fdt_mem_rsv_(const void *fdt, int n)
{
	const struct fdt_reserve_entry *rsv_table =
		(const struct fdt_reserve_entry *)
		((const char *)fdt + fdt_off_mem_rsvmap(fdt));

	return rsv_table + n;
}

static inline struct fdt_reserve_entry *fdt_mem_rsv_w_(void *fdt, int n)
{
	return (void *)(uintptr_t)fdt_mem_rsv_(fdt, n);
}

const void *fdt_offset_ptr(const void *fdt, int offset, unsigned int len)
{
	unsigned int absoffset = offset + fdt_off_dt_struct(fdt);

	if ((absoffset < offset)
			|| ((absoffset + len) < absoffset)
			|| (absoffset + len) > fdt_totalsize(fdt))
		return NULL;

	if (fdt_version(fdt) >= 0x11)
		if (((offset + len) < offset)
				|| ((offset + len) > fdt_size_dt_struct(fdt)))
			return NULL;

	return fdt_offset_ptr_(fdt, offset);
}

uint32_t fdt_next_tag(const void *fdt, int startoffset, int *nextoffset)
{
	const fdt32_t *tagp, *lenp;
	uint32_t tag;
	int offset = startoffset;
	const char *p;

	*nextoffset = -FDT_ERR_TRUNCATED;
	tagp = fdt_offset_ptr(fdt, offset, FDT_TAGSIZE);
	if (!tagp)
		return FDT_END; /* premature end */
	tag = fdt32_to_cpu(*tagp);
	offset += FDT_TAGSIZE;

	*nextoffset = -FDT_ERR_BADSTRUCTURE;
	switch (tag) {
	case FDT_BEGIN_NODE:
		/* skip name */
		do {
			p = fdt_offset_ptr(fdt, offset++, 1);
		} while (p && (*p != '\0'));
		if (!p)
			return FDT_END; /* premature end */
		break;

	case FDT_PROP:
		lenp = fdt_offset_ptr(fdt, offset, sizeof(*lenp));
		if (!lenp)
			return FDT_END; /* premature end */
		/* skip-name offset, length and value */
		offset += sizeof(struct fdt_property) - FDT_TAGSIZE
			+ fdt32_to_cpu(*lenp);
		if (fdt_version(fdt) < 0x10 && fdt32_to_cpu(*lenp) >= 8 &&
				((offset - fdt32_to_cpu(*lenp)) % 8) != 0)
			offset += 4;
		break;

	case FDT_END:
	case FDT_END_NODE:
	case FDT_NOP:
		break;

	default:
		return FDT_END;
	}

	if (!fdt_offset_ptr(fdt, startoffset, offset - startoffset))
		return FDT_END; /* premature end */

	*nextoffset = FDT_TAGALIGN(offset);
	return tag;
}

int fdt_check_node_offset_(const void *fdt, int offset)
{
	if ((offset < 0) || (offset % FDT_TAGSIZE)
	    || (fdt_next_tag(fdt, offset, &offset) != FDT_BEGIN_NODE))
		return -FDT_ERR_BADOFFSET;

	return offset;
}

int fdt_check_prop_offset_(const void *fdt, int offset)
{
	if ((offset < 0) || (offset % FDT_TAGSIZE)
		|| (fdt_next_tag(fdt, offset, &offset) != FDT_PROP))
		return -FDT_ERR_BADOFFSET;

	return offset;
}

int fdt_next_node(const void *fdt, int offset, int *depth)
{
	int nextoffset = 0;
	uint32_t tag;

	if (offset >= 0) {
		nextoffset = fdt_check_node_offset_(fdt, offset);
		if (nextoffset < 0)
			return nextoffset;
	}

	do {
		offset = nextoffset;
		tag = fdt_next_tag(fdt, offset, &nextoffset);

		switch (tag) {
		case FDT_PROP:
		case FDT_NOP:
			break;

		case FDT_BEGIN_NODE:
			if (depth)
				(*depth)++;
			break;

		case FDT_END_NODE:
			if (depth && ((--(*depth)) < 0))
				return nextoffset;
			break;

		case FDT_END:
			if ((nextoffset >= 0)
					|| ((nextoffset == -FDT_ERR_TRUNCATED) && !depth))
				return -FDT_ERR_NOTFOUND;
			else
				return nextoffset;
		}
	} while (tag != FDT_BEGIN_NODE);

	return offset;
}

int fdt_first_subnode(const void *fdt, int offset)
{
	int depth = 0;

	offset = fdt_next_node(fdt, offset, &depth);
	if (offset < 0 || depth != 1)
		return -FDT_ERR_NOTFOUND;

	return offset;
}

int fdt_next_subnode(const void *fdt, int offset)
{
	int depth = 1;

	/*
	 * With respect to the parent, the depth of the next subnode will be
	 * the same as the last.
	 */
	do {
		offset = fdt_next_node(fdt, offset, &depth);
		if (offset < 0 || depth < 1)
			return -FDT_ERR_NOTFOUND;
	} while (depth > 1);

	return offset;
}

const char *fdt_find_string_(const char *strtab, int tabsize, const char *s)
{
	int len = strlen(s) + 1;
	const char *last = strtab + tabsize - len;
	const char *p;

	for (p = strtab; p <= last; p++)
		if (memcmp(p, s, len) == 0)
			return p;
	return NULL;
}

int fdt_move(const void *fdt, void *buf, int bufsize)
{
	int err;

	err = fdt_check_header(fdt);
	if (err != 0)
		return err;

	if (fdt_totalsize(fdt) > bufsize)
		return -FDT_ERR_NOSPACE;

	memmove(buf, fdt, fdt_totalsize(fdt));
	return 0;
}

#define fdt_for_each_subnode(node, fdt, parent)		\
	for (node = fdt_first_subnode(fdt, parent);		\
			node >= 0;								\
			node = fdt_next_subnode(fdt, node))


/**********************************************************************/
/* Read-only functions                                                */
/**********************************************************************/
const char *fdt_get_alias_namelen(const void *fdt, const char *name, int namelen);
const char *fdt_get_name(const void *fdt, int nodeoffset, int *len);
static uint32_t fdt_get_phandle(const void *fdt, int nodeoffset);
static int fdt_path_offset(const void *fdt, const char *path);


static int fdt_nodename_eq_(const void *fdt, int offset,
		const char *s, int len)
{
	int olen;
	const char *p = fdt_get_name(fdt, offset, &olen);

	if (!p || olen < len)
		/* short match */
		return 0;

	if (memcmp(p, s, len) != 0)
		return 0;

	if (p[len] == '\0')
		return 1;
	else if (!memchr(s, '@', len) && (p[len] == '@'))
		return 1;
	else
		return 0;
}

const char *fdt_string(const void *fdt, int stroffset)
{
	return (const char *)fdt + fdt_off_dt_strings(fdt) + stroffset;
}

static int fdt_string_eq_(const void *fdt, int stroffset, const char *s, int len)
{
	const char *p = fdt_string(fdt, stroffset);

	return (strlen(p) == len) && (memcmp(p, s, len) == 0);
}

uint32_t fdt_get_max_phandle(const void *fdt)
{
	uint32_t max_phandle = 0;
	int offset;

	for (offset = fdt_next_node(fdt, -1, NULL); ; offset = fdt_next_node(fdt, offset, NULL)) {
		uint32_t phandle;

		if (offset == -FDT_ERR_NOTFOUND)
			return max_phandle;

		if (offset < 0)
			return (uint32_t)-1;

		phandle = fdt_get_phandle(fdt, offset);
		if (phandle == (uint32_t)-1)
			continue;

		if (phandle > max_phandle)
			max_phandle = phandle;
	}

	return 0;
}

int fdt_num_mem_rsv(const void *fdt)
{
	int i = 0;

	while (fdt64_to_cpu(fdt_mem_rsv_(fdt, i)->size) != 0)
		i++;
	return i;
}

static int nextprop_(const void *fdt, int offset)
{
	uint32_t tag;
	int nextoffset;

	do {
		tag = fdt_next_tag(fdt, offset, &nextoffset);

		switch (tag) {
		case FDT_END:
			if (nextoffset >= 0)
				return -FDT_ERR_BADSTRUCTURE;
			else
				return nextoffset;

		case FDT_PROP:
			return offset;
		}
		offset = nextoffset;
	} while (tag == FDT_NOP);

	return -FDT_ERR_NOTFOUND;
}

int fdt_subnode_offset_namelen(const void *fdt, int offset, const char *name,
		int namelen)
{
	int depth;
	int err;

	err = fdt_check_header(fdt);
	if (err != 0)
		return err;

	for (depth = 0;
			(offset >= 0) && (depth >= 0);
			offset = fdt_next_node(fdt, offset, &depth))
		if ((depth == 1)
				&& fdt_nodename_eq_(fdt, offset, name, namelen))
			return offset;

	if (depth < 0)
		return -FDT_ERR_NOTFOUND;
	return offset; /* error */
}

int fdt_subnode_offset(const void *fdt, int parentoffset, const char *name)
{
	return fdt_subnode_offset_namelen(fdt, parentoffset, name, strlen(name));
}

int fdt_path_offset_namelen(const void *fdt, const char *path, int namelen)
{
	const char *end = path + namelen;
	const char *p = path;
	int offset = 0;
	int err;

	err = fdt_check_header(fdt);
	if (err != 0)
		return err;

	/* see if we have an alias */
	if (*path != '/') {
		const char *q = memchr(path, '/', end - p);

		if (!q)
			q = end;

		p = fdt_get_alias_namelen(fdt, p, q - p);
		if (!p)
			return -FDT_ERR_BADPATH;
		offset = fdt_path_offset(fdt, p);

		p = q;
	}

	while (p < end) {
		const char *q;

		while (*p == '/') {
			p++;
			if (p == end)
				return offset;
		}
		q = memchr(p, '/', end - p);
		if (!q)
			q = end;

		offset = fdt_subnode_offset_namelen(fdt, offset, p, q-p);
		if (offset < 0)
			return offset;

		p = q;
	}

	return offset;
}

static int fdt_path_offset(const void *fdt, const char *path)
{
	return fdt_path_offset_namelen(fdt, path, strlen(path));
}

const char *fdt_get_name(const void *fdt, int nodeoffset, int *len)
{
	const struct fdt_node_header *nh = fdt_offset_ptr_(fdt, nodeoffset);
	const char *nameptr;
	int err;

	err = fdt_check_header(fdt);
	if (err != 0)
		goto fail;
	err = fdt_check_node_offset_(fdt, nodeoffset);
	if (err < 0)
		goto fail;

	nameptr = nh->name;

	if (fdt_version(fdt) < 0x10) {
		/*
		 * For old FDT versions, match the naming conventions of V16:
		 * give only the leaf name (after all /). The actual tree
		 * contents are loosely checked.
		 */
		const char *leaf;

		leaf = strrchr(nameptr, '/');
		if (leaf == NULL) {
			err = -FDT_ERR_BADSTRUCTURE;
			goto fail;
		}
		nameptr = leaf+1;
	}

	if (len)
		*len = strlen(nameptr);

	return nameptr;

fail:
	if (len)
		*len = err;
	return NULL;
}

int fdt_first_property_offset(const void *fdt, int nodeoffset)
{
	int offset;

	offset = fdt_check_node_offset_(fdt, nodeoffset);
	if (offset < 0)
		return offset;

	return nextprop_(fdt, offset);
}

int fdt_next_property_offset(const void *fdt, int offset)
{
	offset = fdt_check_prop_offset_(fdt, offset);
	if (offset < 0)
		return offset;

	return nextprop_(fdt, offset);
}

#define fdt_for_each_property_offset(property, fdt, node)	\
	for (property = fdt_first_property_offset(fdt, node);	\
			property >= 0;					\
			property = fdt_next_property_offset(fdt, property))

static const struct fdt_property *fdt_get_property_by_offset_(const void *fdt,
		int offset, int *lenp)
{
	int err;
	const struct fdt_property *prop;

	err = fdt_check_prop_offset_(fdt, offset);
	if (err < 0) {
		if (lenp)
			*lenp = err;
		return NULL;
	}

	prop = fdt_offset_ptr_(fdt, offset);

	if (lenp)
		*lenp = fdt32_to_cpu(prop->len);

	return prop;
}

const struct fdt_property *fdt_get_property_by_offset(const void *fdt, int offset,
		int *lenp)
{
	/* Prior to version 16, properties may need realignment
	 * and this API does not work. fdt_getprop_*() will, however.
	 */
	if (fdt_version(fdt) < 0x10) {
		if (lenp)
			*lenp = -FDT_ERR_BADVERSION;
		return NULL;
	}

	return fdt_get_property_by_offset_(fdt, offset, lenp);
}

static const struct fdt_property *fdt_get_property_namelen_(const void *fdt,
		int offset, const char *name, int namelen, int *lenp, int *poffset)
{
	for (offset = fdt_first_property_offset(fdt, offset);
			(offset >= 0);
			(offset = fdt_next_property_offset(fdt, offset))) {
		const struct fdt_property *prop;

		prop = fdt_get_property_by_offset_(fdt, offset, lenp);
		if (!prop) {
			offset = -FDT_ERR_INTERNAL;
			break;
		}
		if (fdt_string_eq_(fdt, fdt32_to_cpu(prop->nameoff),
				name, namelen)) {
			if (poffset)
				*poffset = offset;
			return prop;
		}
	}

	if (lenp)
		*lenp = offset;
	return NULL;
}

const struct fdt_property *fdt_get_property_namelen(const void *fdt, int offset,
		const char *name, int namelen, int *lenp)
{
	/* Prior to version 16, properties may need realignment
	 * and this API does not work. fdt_getprop_*() will, however.
	 */
	if (fdt_version(fdt) < 0x10) {
		if (lenp)
			*lenp = -FDT_ERR_BADVERSION;
		return NULL;
	}

	return fdt_get_property_namelen_(fdt, offset, name, namelen, lenp,
					 NULL);
}

const struct fdt_property *fdt_get_property(const void *fdt, int nodeoffset,
		const char *name, int *lenp)
{
	return fdt_get_property_namelen(fdt, nodeoffset, name,
			strlen(name), lenp);
}

static inline struct fdt_property *fdt_get_property_w(void *fdt, int nodeoffset,
		const char *name, int *lenp)
{
	return (struct fdt_property *)(uintptr_t)
		fdt_get_property(fdt, nodeoffset, name, lenp);
}

const void *fdt_getprop_namelen(const void *fdt, int nodeoffset, const char *name,
		int namelen, int *lenp)
{
	int poffset;
	const struct fdt_property *prop;

	prop = fdt_get_property_namelen_(fdt, nodeoffset, name, namelen, lenp,
			&poffset);
	if (!prop)
		return NULL;

	/* Handle realignment */
	if (fdt_version(fdt) < 0x10 && (poffset + sizeof(*prop)) % 8 &&
			fdt32_to_cpu(prop->len) >= 8)
		return prop->data + 4;

	return prop->data;
}

static inline void *fdt_getprop_namelen_w(void *fdt, int nodeoffset, const char *name,
		int namelen, int *lenp)
{
	return (void *)(uintptr_t)fdt_getprop_namelen(fdt, nodeoffset, name,
			namelen, lenp);
}

const void *fdt_getprop_by_offset(const void *fdt, int offset, const char **namep,
		int *lenp)
{
	const struct fdt_property *prop;

	prop = fdt_get_property_by_offset_(fdt, offset, lenp);
	if (!prop)
		return NULL;
	if (namep)
		*namep = fdt_string(fdt, fdt32_to_cpu(prop->nameoff));

	/* Handle realignment */
	if (fdt_version(fdt) < 0x10 && (offset + sizeof(*prop)) % 8 &&
			fdt32_to_cpu(prop->len) >= 8)
		return prop->data + 4;
	return prop->data;
}

const void *fdt_getprop(const void *fdt, int nodeoffset, const char *name, int *lenp)
{
	return fdt_getprop_namelen(fdt, nodeoffset, name, strlen(name), lenp);
}

static inline void *fdt_getprop_w(void *fdt, int nodeoffset, const char *name, int *lenp)
{
	return (void *)(uintptr_t)fdt_getprop(fdt, nodeoffset, name, lenp);
}

static uint32_t fdt_get_phandle(const void *fdt, int nodeoffset)
{
	const fdt32_t *php;
	int len;

	/* FIXME: This is a bit sub-optimal, since we potentially scan
	 * over all the properties twice.
	 */
	php = fdt_getprop(fdt, nodeoffset, "phandle", &len);
	if (!php || (len != sizeof(*php))) {
		php = fdt_getprop(fdt, nodeoffset, "linux,phandle", &len);
		if (!php || (len != sizeof(*php)))
			return 0;
	}

	return fdt32_to_cpu(*php);
}

const char *fdt_get_alias_namelen(const void *fdt, const char *name, int namelen)
{
	int aliasoffset;

	aliasoffset = fdt_path_offset(fdt, "/aliases");
	if (aliasoffset < 0)
		return NULL;

	return fdt_getprop_namelen(fdt, aliasoffset, name, namelen, NULL);
}

const char *fdt_get_alias(const void *fdt, const char *name)
{
	return fdt_get_alias_namelen(fdt, name, strlen(name));
}

int fdt_get_path(const void *fdt, int nodeoffset, char *buf, int buflen)
{
	int pdepth = 0, p = 0;
	int offset, depth, namelen;
	const char *name;
	int err;

	err = fdt_check_header(fdt);
	if (err != 0)
		return err;

	if (buflen < 2)
		return -FDT_ERR_NOSPACE;

	for (offset = 0, depth = 0;
			(offset >= 0) && (offset <= nodeoffset);
			offset = fdt_next_node(fdt, offset, &depth)) {
		while (pdepth > depth) {
			do {
				p--;
			} while (buf[p-1] != '/');
			pdepth--;
		}

		if (pdepth >= depth) {
			name = fdt_get_name(fdt, offset, &namelen);
			if (!name)
				return namelen;
			if ((p + namelen + 1) <= buflen) {
				memcpy(buf + p, name, namelen);
				p += namelen;
				buf[p++] = '/';
				pdepth++;
			}
		}

		if (offset == nodeoffset) {
			if (pdepth < (depth + 1))
				return -FDT_ERR_NOSPACE;

			if (p > 1) /* special case so that root path is "/", not "" */
				p--;
			buf[p] = '\0';
			return 0;
		}
	}

	if ((offset == -FDT_ERR_NOTFOUND) || (offset >= 0))
		return -FDT_ERR_BADOFFSET;
	else if (offset == -FDT_ERR_BADOFFSET)
		return -FDT_ERR_BADSTRUCTURE;

	return offset; /* error from fdt_next_node() */
}

int fdt_supernode_atdepth_offset(const void *fdt, int nodeoffset, int supernodedepth,
		int *nodedepth)
{
	int offset, depth;
	int supernodeoffset = -FDT_ERR_INTERNAL;
	int err;

	err = fdt_check_header(fdt);
	if (err != 0)
		return err;

	if (supernodedepth < 0)
		return -FDT_ERR_NOTFOUND;

	for (offset = 0, depth = 0;
			(offset >= 0) && (offset <= nodeoffset);
			offset = fdt_next_node(fdt, offset, &depth)) {
		if (depth == supernodedepth)
			supernodeoffset = offset;

		if (offset == nodeoffset) {
			if (nodedepth)
				*nodedepth = depth;

			if (supernodedepth > depth)
				return -FDT_ERR_NOTFOUND;
			else
				return supernodeoffset;
		}
	}

	if ((offset == -FDT_ERR_NOTFOUND) || (offset >= 0))
		return -FDT_ERR_BADOFFSET;
	else if (offset == -FDT_ERR_BADOFFSET)
		return -FDT_ERR_BADSTRUCTURE;

	return offset; /* error from fdt_next_node() */
}

int fdt_node_depth(const void *fdt, int nodeoffset)
{
	int nodedepth;
	int err;

	err = fdt_supernode_atdepth_offset(fdt, nodeoffset, 0, &nodedepth);
	if (err)
		return (err < 0) ? err : -FDT_ERR_INTERNAL;
	return nodedepth;
}

int fdt_parent_offset(const void *fdt, int nodeoffset)
{
	int nodedepth = fdt_node_depth(fdt, nodeoffset);

	if (nodedepth < 0)
		return nodedepth;
	return fdt_supernode_atdepth_offset(fdt, nodeoffset,
			nodedepth - 1, NULL);
}

int fdt_node_offset_by_phandle(const void *fdt, uint32_t phandle)
{
	int offset;
	int err;

	if ((phandle == 0) || (phandle == -1))
		return -FDT_ERR_BADPHANDLE;

	err = fdt_check_header(fdt);
	if (err != 0)
		return err;

	/* FIXME: The algorithm here is pretty horrible: we
	 * potentially scan each property of a node in
	 * fdt_get_phandle(), then if that didn't find what
	 * we want, we scan over them again making our way to the next
	 * node.  Still it's the easiest to implement approach;
	 * performance can come later.
	 */
	for (offset = fdt_next_node(fdt, -1, NULL);
			offset >= 0;
			offset = fdt_next_node(fdt, offset, NULL)) {
		if (fdt_get_phandle(fdt, offset) == phandle)
			return offset;
	}

	return offset; /* error from fdt_next_node() */
}


/**********************************************************************/
/* Write-in-place functions                                           */
/**********************************************************************/
int fdt_setprop_inplace_namelen_partial(void *fdt, int nodeoffset, const char *name,
		int namelen, uint32_t idx, const void *val, int len)
{
	void *propval;
	int proplen;

	propval = fdt_getprop_namelen_w(fdt, nodeoffset, name, namelen, &proplen);
	if (!propval)
		return proplen;

	if (proplen < (len + idx))
		return -FDT_ERR_NOSPACE;

	memcpy((char *)propval + idx, val, len);
	return 0;
}

int fdt_setprop_inplace(void *fdt, int nodeoffset, const char *name, const void *val,
		int len)
{
	const void *propval;
	int proplen;

	propval = fdt_getprop(fdt, nodeoffset, name, &proplen);
	if (!propval)
		return proplen;

	if (proplen != len)
		return -FDT_ERR_NOSPACE;

	return fdt_setprop_inplace_namelen_partial(fdt, nodeoffset, name,
			strlen(name), 0, val, len);
}

static inline int fdt_setprop_inplace_u32(void *fdt, int nodeoffset, const char *name,
		uint32_t val)
{
	fdt32_t tmp = cpu_to_fdt32(val);

	return fdt_setprop_inplace(fdt, nodeoffset, name, &tmp, sizeof(tmp));
}


/**********************************************************************/
/* Read-write functions                                               */
/**********************************************************************/
static void fdt_packblocks_(const char *old, char *new, int mem_rsv_size, int struct_size)
{
	int mem_rsv_off, struct_off, strings_off;

	mem_rsv_off = FDT_ALIGN(sizeof(struct fdt_header), 8);
	struct_off = mem_rsv_off + mem_rsv_size;
	strings_off = struct_off + struct_size;

	memmove(new + mem_rsv_off, old + fdt_off_mem_rsvmap(old), mem_rsv_size);
	fdt_set_off_mem_rsvmap(new, mem_rsv_off);

	memmove(new + struct_off, old + fdt_off_dt_struct(old), struct_size);
	fdt_set_off_dt_struct(new, struct_off);
	fdt_set_size_dt_struct(new, struct_size);

	memmove(new + strings_off, old + fdt_off_dt_strings(old),
		fdt_size_dt_strings(old));
	fdt_set_off_dt_strings(new, strings_off);
	fdt_set_size_dt_strings(new, fdt_size_dt_strings(old));
}

static int fdt_blocks_misordered_(const void *fdt, int mem_rsv_size, int struct_size)
{
	return (fdt_off_mem_rsvmap(fdt) < FDT_ALIGN(sizeof(struct fdt_header), 8))
			|| (fdt_off_dt_struct(fdt) <
			(fdt_off_mem_rsvmap(fdt) + mem_rsv_size))
			|| (fdt_off_dt_strings(fdt) <
			(fdt_off_dt_struct(fdt) + struct_size))
			|| (fdt_totalsize(fdt) <
			(fdt_off_dt_strings(fdt) + fdt_size_dt_strings(fdt)));
}

static int fdt_rw_check_header_(void *fdt)
{
	int err;

	err = fdt_check_header(fdt);
	if (err != 0)
		return err;

	if (fdt_version(fdt) < FDT_LAST_SUPPORTED_VERSION)
		return -FDT_ERR_BADVERSION;
	if (fdt_blocks_misordered_(fdt, sizeof(struct fdt_reserve_entry),
				   fdt_size_dt_struct(fdt)))
		return -FDT_ERR_BADLAYOUT;
	if (fdt_version(fdt) > FDT_LAST_SUPPORTED_VERSION)
		fdt_set_version(fdt, FDT_LAST_SUPPORTED_VERSION);

	return 0;
}

static inline int fdt_data_size_(void *fdt)
{
	return fdt_off_dt_strings(fdt) + fdt_size_dt_strings(fdt);
}

static int fdt_splice_(void *fdt, void *splicepoint, int oldlen, int newlen)
{
	char *p = splicepoint;
	char *end = (char *)fdt + fdt_data_size_(fdt);

	if (((p + oldlen) < p) || ((p + oldlen) > end))
		return -FDT_ERR_BADOFFSET;
	if ((p < (char *)fdt) || ((end - oldlen + newlen) < (char *)fdt))
		return -FDT_ERR_BADOFFSET;
	if ((end - oldlen + newlen) > ((char *)fdt + fdt_totalsize(fdt)))
		return -FDT_ERR_NOSPACE;
	memmove(p + newlen, p + oldlen, end - p - oldlen);
	return 0;
}

static int fdt_splice_struct_(void *fdt, void *p, int oldlen, int newlen)
{
	int delta = newlen - oldlen;
	int err;

	err = fdt_splice_(fdt, p, oldlen, newlen);
	if (err)
		return err;

	fdt_set_size_dt_struct(fdt, fdt_size_dt_struct(fdt) + delta);
	fdt_set_off_dt_strings(fdt, fdt_off_dt_strings(fdt) + delta);
	return 0;
}

static int fdt_splice_string_(void *fdt, int newlen)
{
	void *p = (char *)fdt + fdt_off_dt_strings(fdt) + fdt_size_dt_strings(fdt);
	int err;

	err = fdt_splice_(fdt, p, 0, newlen);
	if (err)
		return err;

	fdt_set_size_dt_strings(fdt, fdt_size_dt_strings(fdt) + newlen);
	return 0;
}

static int fdt_find_add_string_(void *fdt, const char *s)
{
	char *strtab = (char *)fdt + fdt_off_dt_strings(fdt);
	const char *p;
	char *new;
	int len = strlen(s) + 1;
	int err;

	p = fdt_find_string_(strtab, fdt_size_dt_strings(fdt), s);
	if (p)
		/* found it */
		return (p - strtab);

	new = strtab + fdt_size_dt_strings(fdt);
	err = fdt_splice_string_(fdt, len);
	if (err)
		return err;

	memcpy(new, s, len);
	return (new - strtab);
}

static int fdt_resize_property_(void *fdt, int nodeoffset, const char *name, int len,
		struct fdt_property **prop)
{
	int oldlen;
	int err;

	*prop = fdt_get_property_w(fdt, nodeoffset, name, &oldlen);
	if (!*prop)
		return oldlen;

	err = fdt_splice_struct_(fdt, (*prop)->data, FDT_TAGALIGN(oldlen), FDT_TAGALIGN(len));
	if (err)
		return err;

	(*prop)->len = cpu_to_fdt32(len);
	return 0;
}

static int fdt_add_property_(void *fdt, int nodeoffset, const char *name, int len,
		struct fdt_property **prop)
{
	int proplen;
	int nextoffset;
	int namestroff;
	int err;

	nextoffset = fdt_check_node_offset_(fdt, nodeoffset);
	if (nextoffset < 0)
		return nextoffset;

	namestroff = fdt_find_add_string_(fdt, name);
	if (namestroff < 0)
		return namestroff;

	*prop = fdt_offset_ptr_w_(fdt, nextoffset);
	proplen = sizeof(**prop) + FDT_TAGALIGN(len);

	err = fdt_splice_struct_(fdt, *prop, 0, proplen);
	if (err)
		return err;

	(*prop)->tag = cpu_to_fdt32(FDT_PROP);
	(*prop)->nameoff = cpu_to_fdt32(namestroff);
	(*prop)->len = cpu_to_fdt32(len);
	return 0;
}

int fdt_setprop_placeholder(void *fdt, int nodeoffset, const char *name, int len,
		void **prop_data)
{
	struct fdt_property *prop;
	int err;

	err = fdt_rw_check_header_(fdt);
	if (err != 0)
		return err;

	err = fdt_resize_property_(fdt, nodeoffset, name, len, &prop);
	if (err == -FDT_ERR_NOTFOUND)
		err = fdt_add_property_(fdt, nodeoffset, name, len, &prop);
	if (err)
		return err;

	*prop_data = prop->data;
	return 0;
}

int fdt_setprop(void *fdt, int nodeoffset, const char *name, const void *val, int len)
{
	void *prop_data;
	int err;

	err = fdt_setprop_placeholder(fdt, nodeoffset, name, len, &prop_data);
	if (err)
		return err;

	if (len)
		memcpy(prop_data, val, len);
	return 0;
}

int fdt_add_subnode_namelen(void *fdt, int parentoffset, const char *name, int namelen)
{
	struct fdt_node_header *nh;
	int offset, nextoffset;
	int nodelen;
	int err;
	uint32_t tag;
	fdt32_t *endtag;

	err = fdt_rw_check_header_(fdt);
	if (err != 0)
		return err;

	offset = fdt_subnode_offset_namelen(fdt, parentoffset, name, namelen);
	if (offset >= 0)
		return -FDT_ERR_EXISTS;
	else if (offset != -FDT_ERR_NOTFOUND)
		return offset;

	/* Try to place the new node after the parent's properties */
	tag = fdt_next_tag(fdt, parentoffset, &nextoffset); /* skip the BEGIN_NODE */
	do {
		offset = nextoffset;
		tag = fdt_next_tag(fdt, offset, &nextoffset);
	} while ((tag == FDT_PROP) || (tag == FDT_NOP));

	nh = fdt_offset_ptr_w_(fdt, offset);
	nodelen = sizeof(*nh) + FDT_TAGALIGN(namelen+1) + FDT_TAGSIZE;

	err = fdt_splice_struct_(fdt, nh, 0, nodelen);
	if (err)
		return err;

	nh->tag = cpu_to_fdt32(FDT_BEGIN_NODE);
	memset(nh->name, 0, FDT_TAGALIGN(namelen+1));
	memcpy(nh->name, name, namelen);
	endtag = (fdt32_t *)((char *)nh + nodelen - FDT_TAGSIZE);
	*endtag = cpu_to_fdt32(FDT_END_NODE);

	return offset;
}

int fdt_add_subnode(void *fdt, int parentoffset, const char *name)
{
	return fdt_add_subnode_namelen(fdt, parentoffset, name, strlen(name));
}

int fdt_open_into(const void *fdt, void *buf, int bufsize)
{
	int err = -1;
	uint32_t mem_rsv_size;
	int struct_size;
	uint32_t newsize;
	const char *fdtstart = fdt;
	const char *fdtend = NULL;
	char *tmp;

	if (fdtstart + fdt_totalsize(fdt) < fdtstart)
		return err;

	fdtend = fdtstart + fdt_totalsize(fdt);

	err = fdt_check_header(fdt);
	if (err != 0)
		return err;

	if ((fdt_num_mem_rsv(fdt)+1) > (UINT_MAX / sizeof(struct fdt_reserve_entry)))
		return err;

	mem_rsv_size = (fdt_num_mem_rsv(fdt)+1)
		* sizeof(struct fdt_reserve_entry);

	if (fdt_version(fdt) >= 17) {
		struct_size = fdt_size_dt_struct(fdt);
		if (struct_size < 0)
			return struct_size;
	} else {
		struct_size = 0;
		while (fdt_next_tag(fdt, struct_size, &struct_size) != FDT_END)
			;
		if (struct_size < 0)
			return struct_size;
	}

	if (!fdt_blocks_misordered_(fdt, mem_rsv_size, struct_size)) {
		/* no further work necessary */
		err = fdt_move(fdt, buf, bufsize);
		if (err)
			return err;
		fdt_set_version(buf, 17);
		fdt_set_size_dt_struct(buf, struct_size);
		fdt_set_totalsize(buf, bufsize);
		return 0;
	}

	if (((uint64_t)FDT_ALIGN(sizeof(struct fdt_header), 8) + (uint64_t)mem_rsv_size
		+ (uint64_t)struct_size + (uint64_t)fdt_size_dt_strings(fdt)) > UINT_MAX) {
		return (err = -1);
	}

	/* Need to reorder */
	newsize = FDT_ALIGN(sizeof(struct fdt_header), 8) + mem_rsv_size
		+ struct_size + fdt_size_dt_strings(fdt);

	if (bufsize < newsize)
		return -FDT_ERR_NOSPACE;

	/* First attempt to build converted tree at beginning of buffer */
	tmp = buf;
	if (((tmp + newsize) < tmp) || ((buf + bufsize) < buf)) {
		return (err = -1);
	}
	/* But if that overlaps with the old tree... */
	if (((tmp + newsize) > fdtstart) && (tmp < fdtend)) {
		/* Try right after the old tree instead */
		tmp = (char *)(uintptr_t)fdtend;
		if ((tmp + newsize) > ((char *)buf + bufsize))
			return -FDT_ERR_NOSPACE;
	}

	fdt_packblocks_(fdt, tmp, mem_rsv_size, struct_size);
	memmove(buf, tmp, newsize);

	fdt_set_magic(buf, FDT_MAGIC);
	fdt_set_totalsize(buf, bufsize);
	fdt_set_version(buf, 17);
	fdt_set_last_comp_version(buf, 16);
	fdt_set_boot_cpuid_phys(buf, fdt_boot_cpuid_phys(fdt));

	return 0;
}

static uint32_t overlay_get_target_phandle(const void *fdto, int fragment)
{
	const fdt32_t *val;
	int len;

	val = fdt_getprop(fdto, fragment, "target", &len);
	if (!val)
		return 0;

	if ((len != sizeof(*val)) || (fdt32_to_cpu(*val) == (uint32_t)-1))
		return (uint32_t)-1;

	return fdt32_to_cpu(*val);
}

static int overlay_get_target(const void *fdt, const void *fdto, int fragment,
		char const **pathp)
{
	uint32_t phandle;
	const char *path = NULL;
	int path_len = 0, ret;

	/* Try first to do a phandle based lookup */
	phandle = overlay_get_target_phandle(fdto, fragment);
	if (phandle == (uint32_t)-1)
		return -FDT_ERR_BADPHANDLE;

	/* no phandle, try path */
	if (!phandle) {
		/* And then a path based lookup */
		path = fdt_getprop(fdto, fragment, "target-path", &path_len);
		if (path)
			ret = fdt_path_offset(fdt, path);
		else
			ret = path_len;
	} else
		ret = fdt_node_offset_by_phandle(fdt, phandle);

	/* If we haven't found either a target or a
	 * target-path property in a node that contains a
	 * __overlay__ subnode (we wouldn't be called
	 * otherwise), consider it a improperly written
	 * overlay
	 */
	if (ret < 0 && path_len == -FDT_ERR_NOTFOUND)
		ret = -FDT_ERR_BADOVERLAY;

	/* return on error */
	if (ret < 0)
		return ret;

	/* return pointer to path (if available) */
	if (pathp)
		*pathp = path ? path : NULL;

	return ret;
}

static int overlay_phandle_add_offset(void *fdt, int node, const char *name, uint32_t delta)
{
	const fdt32_t *val;
	uint32_t adj_val;
	int len;

	val = fdt_getprop(fdt, node, name, &len);
	if (!val)
		return len;

	if (len != sizeof(*val))
		return -FDT_ERR_BADPHANDLE;

	adj_val = fdt32_to_cpu(*val);
	if ((adj_val + delta) < adj_val)
		return -FDT_ERR_NOPHANDLES;

	adj_val += delta;
	if (adj_val == (uint32_t)-1)
		return -FDT_ERR_NOPHANDLES;

	return fdt_setprop_inplace_u32(fdt, node, name, adj_val);
}

static int overlay_adjust_node_phandles(void *fdto, int node, uint32_t delta)
{
	int child;
	int ret;

	ret = overlay_phandle_add_offset(fdto, node, "phandle", delta);
	if (ret && ret != -FDT_ERR_NOTFOUND)
		return ret;

	ret = overlay_phandle_add_offset(fdto, node, "linux,phandle", delta);
	if (ret && ret != -FDT_ERR_NOTFOUND)
		return ret;

	fdt_for_each_subnode(child, fdto, node) {
		ret = overlay_adjust_node_phandles(fdto, child, delta);
		if (ret)
			return ret;
	}

	return 0;
}

static int overlay_adjust_local_phandles(void *fdto, uint32_t delta)
{
	/*
	 * Start adjusting the phandles from the overlay root
	 */
	return overlay_adjust_node_phandles(fdto, 0, delta);
}

static int overlay_update_local_node_references(void *fdto, int tree_node, int fixup_node,
		uint32_t delta)
{
	int fixup_prop;
	int fixup_child;
	int ret;

	fdt_for_each_property_offset(fixup_prop, fdto, fixup_node) {
		const fdt32_t *fixup_val;
		const char *tree_val;
		const char *name;
		int fixup_len;
		int tree_len;
		int i;

		fixup_val = fdt_getprop_by_offset(fdto, fixup_prop, &name, &fixup_len);
		if (!fixup_val)
			return fixup_len;

		if (fixup_len % sizeof(uint32_t))
			return -FDT_ERR_BADOVERLAY;

		tree_val = fdt_getprop(fdto, tree_node, name, &tree_len);
		if (!tree_val) {
			if (tree_len == -FDT_ERR_NOTFOUND)
				return -FDT_ERR_BADOVERLAY;

			return tree_len;
		}

		for (i = 0; i < (fixup_len / sizeof(uint32_t)); i++) {
			fdt32_t adj_val;
			uint32_t poffset;

			poffset = fdt32_to_cpu(fixup_val[i]);

			/*
			 * phandles to fixup can be unaligned.
			 *
			 * Use a memcpy for the architectures that do
			 * not support unaligned accesses.
			 */
			memcpy(&adj_val, tree_val + poffset, sizeof(adj_val));

			adj_val = cpu_to_fdt32(fdt32_to_cpu(adj_val) + delta);

			ret = fdt_setprop_inplace_namelen_partial(fdto, tree_node, name,
					strlen(name), poffset, &adj_val, sizeof(adj_val));
			if (ret == -FDT_ERR_NOSPACE)
				return -FDT_ERR_BADOVERLAY;

			if (ret)
				return ret;
		}
	}

	fdt_for_each_subnode(fixup_child, fdto, fixup_node) {
		const char *fixup_child_name = fdt_get_name(fdto, fixup_child, NULL);
		int tree_child;

		tree_child = fdt_subnode_offset(fdto, tree_node, fixup_child_name);
		if (tree_child == -FDT_ERR_NOTFOUND)
			return -FDT_ERR_BADOVERLAY;
		if (tree_child < 0)
			return tree_child;

		ret = overlay_update_local_node_references(fdto, tree_child, fixup_child,
				delta);
		if (ret)
			return ret;
	}

	return 0;
}

static int overlay_update_local_references(void *fdto, uint32_t delta)
{
	int fixups;

	fixups = fdt_path_offset(fdto, "/__local_fixups__");
	if (fixups < 0) {
		/* There's no local phandles to adjust, bail out */
		if (fixups == -FDT_ERR_NOTFOUND)
			return 0;

		return fixups;
	}

	/*
	 * Update our local references from the root of the tree
	 */
	return overlay_update_local_node_references(fdto, 0, fixups, delta);
}

static int overlay_fixup_one_phandle(void *fdt, void *fdto, int symbols_off,
		const char *path, uint32_t path_len, const char *name, uint32_t name_len,
		int poffset, const char *label)
{
	const char *symbol_path;
	uint32_t phandle;
	fdt32_t phandle_prop;
	int symbol_off, fixup_off;
	int prop_len;

	if (symbols_off < 0)
		return symbols_off;

	symbol_path = fdt_getprop(fdt, symbols_off, label,
				  &prop_len);
	if (!symbol_path)
		return prop_len;

	symbol_off = fdt_path_offset(fdt, symbol_path);
	if (symbol_off < 0)
		return symbol_off;

	phandle = fdt_get_phandle(fdt, symbol_off);
	if (!phandle)
		return -FDT_ERR_NOTFOUND;

	fixup_off = fdt_path_offset_namelen(fdto, path, path_len);
	if (fixup_off == -FDT_ERR_NOTFOUND)
		return -FDT_ERR_BADOVERLAY;
	if (fixup_off < 0)
		return fixup_off;

	phandle_prop = cpu_to_fdt32(phandle);
	return fdt_setprop_inplace_namelen_partial(fdto, fixup_off, name, name_len, poffset,
			&phandle_prop, sizeof(phandle_prop));
};

#define KSTRTO_BASE 10

static int overlay_fixup_phandle(void *fdt, void *fdto, int symbols_off, int property)
{
	const char *value;
	const char *label;
	int len;

	value = fdt_getprop_by_offset(fdto, property, &label, &len);
	if (!value) {
		if (len == -FDT_ERR_NOTFOUND)
			return -FDT_ERR_INTERNAL;

		return len;
	}

	do {
		const char *path, *name, *fixup_end;
		const char *fixup_str = value;
		uint32_t path_len, name_len;
		uint32_t fixup_len;
		char *sep;
		int ret;
		unsigned long res;

		fixup_end = memchr(value, '\0', len);
		if (!fixup_end)
			return -FDT_ERR_BADOVERLAY;
		fixup_len = fixup_end - fixup_str;

		len -= fixup_len + 1;
		value += fixup_len + 1;

		path = fixup_str;
		sep = memchr(fixup_str, ':', fixup_len);
		if (!sep || *sep != ':')
			return -FDT_ERR_BADOVERLAY;

		path_len = sep - path;
		if (path_len == (fixup_len - 1))
			return -FDT_ERR_BADOVERLAY;

		fixup_len -= path_len + 1;
		name = sep + 1;
		sep = memchr(name, ':', fixup_len);
		if (!sep || *sep != ':')
			return -FDT_ERR_BADOVERLAY;

		name_len = sep - name;
		if (!name_len)
			return -FDT_ERR_BADOVERLAY;

		ret = kstrtoul(sep + 1, KSTRTO_BASE, &res);
		if (ret != 0)
			return -FDT_ERR_BADOVERLAY;
		ret = overlay_fixup_one_phandle(fdt, fdto, symbols_off, path, path_len,
				name, name_len, (int)res, label);
		if (ret)
			return ret;
	} while (len > 0);

	return 0;
}

static int overlay_fixup_phandles(void *fdt, void *fdto)
{
	int fixups_off, symbols_off;
	int property;

	/* We can have overlays without any fixups */
	fixups_off = fdt_path_offset(fdto, "/__fixups__");
	if (fixups_off == -FDT_ERR_NOTFOUND)
		return 0; /* nothing to do */
	if (fixups_off < 0)
		return fixups_off;

	/* And base DTs without symbols */
	symbols_off = fdt_path_offset(fdt, "/__symbols__");
	if ((symbols_off < 0 && (symbols_off != -FDT_ERR_NOTFOUND)))
		return symbols_off;

	fdt_for_each_property_offset(property, fdto, fixups_off) {
		int ret;

		ret = overlay_fixup_phandle(fdt, fdto, symbols_off, property);
		if (ret)
			return ret;
	}

	return 0;
}

static int overlay_apply_node(void *fdt, int target, void *fdto, int node)
{
	int property;
	int subnode;

	fdt_for_each_property_offset(property, fdto, node) {
		const char *name;
		const void *prop;
		int prop_len;
		int ret;

		prop = fdt_getprop_by_offset(fdto, property, &name,
					     &prop_len);
		if (prop_len == -FDT_ERR_NOTFOUND)
			return -FDT_ERR_INTERNAL;
		if (prop_len < 0)
			return prop_len;

		ret = fdt_setprop(fdt, target, name, prop, prop_len);
		if (ret)
			return ret;
	}

	fdt_for_each_subnode(subnode, fdto, node) {
		const char *name = fdt_get_name(fdto, subnode, NULL);
		int nnode;
		int ret;

		nnode = fdt_add_subnode(fdt, target, name);
		if (nnode == -FDT_ERR_EXISTS) {
			nnode = fdt_subnode_offset(fdt, target, name);
			if (nnode == -FDT_ERR_NOTFOUND)
				return -FDT_ERR_INTERNAL;
		}

		if (nnode < 0)
			return nnode;

		ret = overlay_apply_node(fdt, nnode, fdto, subnode);
		if (ret)
			return ret;
	}

	return 0;
}

static int overlay_merge(void *fdt, void *fdto)
{
	int fragment;

	fdt_for_each_subnode(fragment, fdto, 0) {
		int overlay;
		int target;
		int ret;

		/*
		 * Each fragments will have an __overlay__ node. If
		 * they don't, it's not supposed to be merged
		 */
		overlay = fdt_subnode_offset(fdto, fragment, "__overlay__");
		if (overlay == -FDT_ERR_NOTFOUND)
			continue;

		if (overlay < 0)
			return overlay;

		target = overlay_get_target(fdt, fdto, fragment, NULL);
		if (target < 0)
			return target;

		ret = overlay_apply_node(fdt, target, fdto, overlay);
		if (ret)
			return ret;
	}

	return 0;
}

static int get_path_len(const void *fdt, int nodeoffset)
{
	int len = 0, namelen;
	const char *name;
	int err;

	err = fdt_check_header(fdt);
	if (err != 0)
		return err;

	for (;;) {
		name = fdt_get_name(fdt, nodeoffset, &namelen);
		if (!name)
			return namelen;

		/* root? we're done */
		if (namelen == 0)
			break;

		nodeoffset = fdt_parent_offset(fdt, nodeoffset);
		if (nodeoffset < 0)
			return nodeoffset;
		len += namelen + 1;
	}

	/* in case of root pretend it's "/" */
	if (len == 0)
		len++;
	return len;
}

static int overlay_symbol_update_(void *fdt, void *fdto, int root_sym, int ov_sym)
{
	int prop, path_len, fragment, target;
	int len, frag_name_len, ret, rel_path_len;
	const char *s, *e;
	const char *path;
	const char *name;
	const char *frag_name;
	const char *rel_path;
	const char *target_path;
	char *buf;
	void *p;

	/* iterate over each overlay symbol */
	fdt_for_each_property_offset(prop, fdto, ov_sym) {
		path = fdt_getprop_by_offset(fdto, prop, &name, &path_len);
		if (!path)
			return path_len;

		/* verify it's a string property (terminated by a single \0) */
		if (path_len < 1 || memchr(path, '\0', path_len) != &path[path_len - 1])
			return -FDT_ERR_BADVALUE;

		/* keep end marker to avoid strlen() */
		e = path + path_len;

		/* format: /<fragment-name>/__overlay__/<relative-subnode-path> */

		if (*path != '/')
			return -FDT_ERR_BADVALUE;

		/* get fragment name first */
		s = strchr(path + 1, '/');
		if (!s)
			return -FDT_ERR_BADOVERLAY;

		frag_name = path + 1;
		frag_name_len = s - path - 1;

		/* verify format; safe since "s" lies in \0 terminated prop */
		len = sizeof("/__overlay__/") - 1;
		if ((e - s) < len || memcmp(s, "/__overlay__/", len))
			return -FDT_ERR_BADOVERLAY;

		rel_path = s + len;
		rel_path_len = e - rel_path;

		/* find the fragment index in which the symbol lies */
		ret = fdt_subnode_offset_namelen(fdto, 0, frag_name, frag_name_len);
		/* not found? */
		if (ret < 0)
			return -FDT_ERR_BADOVERLAY;
		fragment = ret;

		/* an __overlay__ subnode must exist */
		ret = fdt_subnode_offset(fdto, fragment, "__overlay__");
		if (ret < 0)
			return -FDT_ERR_BADOVERLAY;

		/* get the target of the fragment */
		ret = overlay_get_target(fdt, fdto, fragment, &target_path);
		if (ret < 0)
			return ret;
		target = ret;

		/* if we have a target path use */
		if (!target_path) {
			ret = get_path_len(fdt, target);
			if (ret < 0)
				return ret;
			len = ret;
		} else {
			len = strlen(target_path);
		}

		ret = fdt_setprop_placeholder(fdt, root_sym, name,
				len + (len > 1) + rel_path_len + 1, &p);
		if (ret < 0)
			return ret;

		if (!target_path) {
			/* again in case setprop_placeholder changed it */
			ret = overlay_get_target(fdt, fdto, fragment, &target_path);
			if (ret < 0)
				return ret;
			target = ret;
		}

		buf = p;
		if (len > 1) { /* target is not root */
			if (!target_path) {
				ret = fdt_get_path(fdt, target, buf, len + 1);
				if (ret < 0)
					return ret;
			} else
				memcpy(buf, target_path, len + 1);

		} else
			len--;

		buf[len] = '/';
		memcpy(buf + len + 1, rel_path, rel_path_len);
		buf[len + 1 + rel_path_len] = '\0';
	}

	return 0;
}

static int overlay_symbol_update(void *fdt, void *fdto)
{
	int root_sym, ov_sym;

	ov_sym = fdt_subnode_offset(fdto, 0, "__symbols__");

	/* if no overlay symbols exist no problem */
	if (ov_sym < 0)
		return 0;

	root_sym = fdt_subnode_offset(fdt, 0, "__symbols__");

	/* it no root symbols exist we should create them */
	if (root_sym == -FDT_ERR_NOTFOUND)
		root_sym = fdt_add_subnode(fdt, 0, "__symbols__");

	/* any error is fatal now */
	if (root_sym < 0)
		return root_sym;

	return overlay_symbol_update_(fdt, fdto, root_sym, ov_sym);
}

int fdt_overlay_apply(void *fdt, void *fdto)
{
	uint32_t delta = fdt_get_max_phandle(fdt);
	int ret;
	int err;

	err = fdt_check_header(fdt);
	if (err != 0)
		return err;
	err = fdt_check_header(fdto);
	if (err != 0)
		return err;

	ret = overlay_adjust_local_phandles(fdto, delta);
	if (ret)
		goto err;

	ret = overlay_update_local_references(fdto, delta);
	if (ret)
		goto err;

	ret = overlay_fixup_phandles(fdt, fdto);
	if (ret)
		goto err;

	ret = overlay_merge(fdt, fdto);
	if (ret)
		goto err;

	ret = overlay_symbol_update(fdt, fdto);
	if (ret)
		goto err;

	/*
	 * The overlay has been damaged, erase its magic.
	 */
	fdt_set_magic(fdto, ~0);

	return 0;

err:
	/*
	 * The overlay might have been damaged, erase its magic.
	 */
	fdt_set_magic(fdto, ~0);

	/*
	 * The base device tree might have been damaged, erase its
	 * magic.
	 */
	fdt_set_magic(fdt, ~0);

	return ret;
}
