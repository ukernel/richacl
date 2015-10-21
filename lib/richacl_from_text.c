/*
  Copyright (C) 2006, 2009, 2010  Novell, Inc.
  Copyright (C) 2015  Red Hat, Inc.
  Written by Andreas Gruenbacher <agruenba@redhat.com>

  The richacl library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  The richacl library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see
  <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <alloca.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>

#include "sys/richacl.h"
#include "richacl-internal.h"

static int acl_flags_from_text(const char *str, struct richacl *acl,
			       void (*error)(const char *, ...))
{
	char *dup, *end;

	end = alloca(strlen(str) + 1);
	strcpy(end, str);

	acl->a_flags = 0;
	while ((dup = end)) {
		char *c;
		unsigned long l;
		int i;

		while (*dup == '/')
			dup++;
		end = strchr(dup, '/');
		if (end)
			*end++ = 0;
		if (!*dup)
			break;

		l = strtoul(str, &c, 0);
		if (*c == 0) {
			acl->a_flags |= l;
			continue;
		}

		/* Recognize flag mnemonics */
		for (i = 0; i < acl_flag_bits_size; i++) {
			if (!strcasecmp(dup, acl_flag_bits[i].a_name)) {
				acl->a_flags |= acl_flag_bits[i].a_flag;
				break;
			}
		}
		if (i != acl_flag_bits_size)
			continue;

		/* Recognize single-character flags */
		for (c = dup; *c; c++) {
			if (*c == '-')
				continue;
			for (i = 0; i < acl_flag_bits_size; i++) {
				if (*c == acl_flag_bits[i].a_char) {
					acl->a_flags |= acl_flag_bits[i].a_flag;
					break;
				}
			}
			if (i != acl_flag_bits_size)
				continue;

			error("Invalid acl flag `%s'\n", c);
			return -1;
		}
	}

	return 0;
}

static int identifier_from_text(const char *str, struct richace *ace,
				void (*error)(const char *, ...))
{
	char *c;
	unsigned long l;

	if (ace->e_flags & RICHACE_UNMAPPED_WHO) {
		int ret = richace_set_unmapped_who(ace, str, ace->e_flags);
		if (ret)
			error("%s", strerror(errno));
		return ret;
	}

	c = strchr(str, '@');
	if (c) {
		char *dup;

		if (c[1]) {
			error("Domain name not supported in `%s'\n", str);
			goto fail;
		}

		/* Ignore case in special identifiers. */
		dup = alloca(strlen(str) + 1);
		strcpy(dup, str);
		for (c = dup; *c; c++)
			*c = toupper(*c);

		if (richace_set_special_who(ace, dup)) {
			error("Special user `%s' not supported\n", str);
			goto fail;
		}
		return 0;
	}

	l = strtoul(str, &c, 0);
	if (*c == 0) {
		ace->e_id = l;
		return 0;
	}

	if (ace->e_flags & RICHACE_IDENTIFIER_GROUP) {
		struct group *group = getgrnam(str);

		if (!group) {
			error("Group `%s' does not exist\n", str);
			goto fail;
		}
		ace->e_id = group->gr_gid;
		return 0;
	} else {
		struct passwd *passwd = getpwnam(str);

		if (!passwd) {
			error("User `%s' does not exist\n", str);
			goto fail;
		}
		ace->e_id = passwd->pw_uid;
		return 0;
	}

fail:
	return -1;
}

static int type_from_text(const char *str, struct richace *ace,
			  void (*error)(const char *, ...))
{
	char *c;
	int i;
	unsigned long l;

	l = strtoul(str, &c, 0);
	if (*c == 0) {
		ace->e_type = l;
		return 0;
	}

	/* Recognize type mnemonic */
	for (i = 0; i < type_values_size; i++) {
		if (!strcasecmp(str, type_values[i].e_name)) {
			ace->e_type = type_values[i].e_type;
			return 0;
		}
	}
	error("Invalid entry type `%s'\n", str);
	return -1;
}

static int ace_flags_from_text(const char *str, struct richace *ace,
			       void (*error)(const char *, ...))
{
	char *dup, *end;

	end = alloca(strlen(str) + 1);
	strcpy(end, str);

	ace->e_flags = 0;
	while ((dup = end)) {
		char *c;
		unsigned long l;
		int i;

		while (*dup == '/')
			dup++;
		end = strchr(dup, '/');
		if (end)
			*end++ = 0;
		if (!*dup)
			break;

		l = strtoul(str, &c, 0);
		if (*c == 0) {
			ace->e_flags |= l;
			continue;
		}

		/* Recognize flag mnemonics */
		for (i = 0; i < ace_flag_bits_size; i++) {
			if (!strcasecmp(dup, ace_flag_bits[i].e_name)) {
				ace->e_flags |= ace_flag_bits[i].e_flag;
				break;
			}
		}
		if (i != ace_flag_bits_size)
			continue;

		/* Recognize single-character flags */
		for (c = dup; *c; c++) {
			if (*c == '-')
				continue;
			for (i = 0; i < ace_flag_bits_size; i++) {
				if (*c == ace_flag_bits[i].e_char) {
					ace->e_flags |= ace_flag_bits[i].e_flag;
					break;
				}
			}
			if (i != ace_flag_bits_size)
				continue;

			error("Invalid entry flag `%s'\n", c);
			return -1;
		}
	}

	return 0;
}

static int mask_from_text(const char *str, unsigned int *mask,
			  void (*error)(const char *, ...))
{
	char *dup, *end;

	end = alloca(strlen(str) + 1);
	strcpy(end, str);

	*mask = 0;
	while ((dup = end)) {
		char *c;
		unsigned long l;
		int i;

		while (*dup == '/')
			dup++;
		end = strchr(dup, '/');
		if (end)
			*end++ = 0;
		if (!*dup)
			break;

		l = strtoul(dup, &c, 0);
		if (*c == 0) {
			*mask |= l;
			continue;
		}

		/* Recognize mask mnemonics */
		for (i = 0; i < mask_flags_size; i++) {
			if (!strcasecmp(dup, mask_flags[i].e_name)) {
				*mask |= mask_flags[i].e_mask;
				break;
			}
		}
		if (i != mask_flags_size)
			continue;

		/* Recognize single-character masks */
		for (c = dup; *c; c++) {
			if (*c == '-')
				continue;
			for (i = 0; i < mask_flags_size; i++) {
				if (*c == mask_flags[i].e_char) {
					*mask |= mask_flags[i].e_mask;
					break;
				}
			}
			if (i != mask_flags_size)
				continue;

			error("Invalid access mask `%s'\n", dup);
			return -1;
		}
	}

	return 0;
}

struct richacl *richacl_from_text(const char *str, int *pflags,
				  void (*error)(const char *, ...))
{
	char *who_str = NULL, *mask_str = NULL, *flags_str = NULL,
	     *type_str = NULL;
	struct richacl *acl;
	int flags = 0;

	acl = richacl_alloc(0);
	if (!acl)
		return NULL;

	while (*str) {
		struct richacl *acl2;
		struct richace *ace;
		unsigned int mask;
		const char *entry, *c;

		while (isspace(*str) || *str == ',')
			str++;
		if (!*str)
			break;

		entry = str;
		c = strchr(str, ':');
		if (!c)
			goto fail_syntax;
		who_str = strndup(str, c - str);
		if (!who_str)
			goto fail;
		str = c + 1;

		if (!strcasecmp(who_str, "FLAGS")) {
			for (c = str; *c; c++) {
				if (*c == ':' || *c == ',' || isspace(*c))
					break;
			}
			mask_str = strndup(str, c - str);
			if (!mask_str)
				goto fail;
			if (*c != ':') {
				if (acl_flags_from_text(mask_str, acl, error))
					goto fail_einval;
				flags |= RICHACL_TEXT_FLAGS;
				str = c;
				goto free_mask_str;
			} else {
				free(mask_str);
				mask_str = NULL;
			}
		}

		c = strchr(str, ':');
		if (!c)
			goto fail_syntax;
		mask_str = strndup(str, c - str);
		if (!mask_str)
			goto fail;
		str = c + 1;

		c = strchr(str, ':');
		if (!c)
			goto fail_syntax;
		flags_str = strndup(str, c - str);
		if (!flags_str)
			goto fail;
		str = c + 1;

		for (c = str; *c; c++) {
			if (*c == ',' || isspace(*c))
				break;
		}
		type_str = strndup(str, c - str);
		if (!type_str)
			goto fail;
		str = c;

		if (mask_from_text(mask_str, &mask, error))
			goto fail_einval;
		if (!strcasecmp(type_str, "MASK")) {
			if (!strcasecmp(who_str, "OWNER")) {
				acl->a_owner_mask = mask;
				flags |= RICHACL_TEXT_OWNER_MASK;
			} else if (!strcasecmp(who_str, "GROUP")) {
				acl->a_group_mask = mask;
				flags |= RICHACL_TEXT_GROUP_MASK;
			} else if (!strcasecmp(who_str, "OTHER")) {
				acl->a_other_mask = mask;
				flags |= RICHACL_TEXT_OTHER_MASK;
			} else {
				error("Invalid file mask `%s'\n",
				      who_str);
				goto fail_einval;
			}
		} else {
			size_t size = sizeof(struct richacl) + (acl->a_count
				      + 1) * sizeof(struct richace);
			acl2 = realloc(acl, size);
			if (!acl2)
				goto fail;
			acl = acl2;
			memset(acl->a_entries + acl->a_count, 0,
			       sizeof(struct richace));
			acl->a_count++;

			ace = acl->a_entries + acl->a_count - 1;
			ace->e_mask = mask;
			if (ace_flags_from_text(flags_str, ace, error))
				goto fail_einval;
			if (identifier_from_text(who_str, ace, error))
				goto fail_einval;
			if (type_from_text(type_str, ace, error))
				goto fail_einval;
		}

		free(type_str);
		type_str = NULL;
		free(flags_str);
		flags_str = NULL;
	free_mask_str:
		free(mask_str);
		mask_str = NULL;
		free(who_str);
		who_str = NULL;
		continue;

	fail_syntax:
		for (c = entry; *c && !(isspace(*c) || *c == ','); c++)
			;
		error("Invalid entry `%.*s'\n", c - entry, entry);
		goto fail_einval;
	}

	if (pflags)
		*pflags = flags;
	return acl;

fail_einval:
	errno = EINVAL;

fail:
	free(type_str);
	free(flags_str);
	free(mask_str);
	free(who_str);
	richacl_free(acl);
	return NULL;
}