/*
  Copyright (C) 1999, 2000 Novell, Inc.
  Copyright (C) 2015  Red Hat, Inc.
  Written by Andreas Gruenbacher <agruenba@redhat.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see
  <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdio.h>
#include "user_group.h"


const char *
user_name(uid_t uid, int numeric)
{
	struct passwd *passwd = numeric ? NULL : getpwuid(uid);
	static char uid_str[22];
	int ret;

	if (passwd != NULL)
		return passwd->pw_name;
	ret = snprintf(uid_str, sizeof(uid_str), "%ld", (long)uid);
	if (ret < 1 || (size_t)ret >= sizeof(uid_str))
		return "?";
	return uid_str;
}


const char *
group_name(gid_t gid, int numeric)
{
	struct group *group = numeric ? NULL : getgrgid(gid);
	static char gid_str[22];
	int ret;

	if (group != NULL)
		return group->gr_name;
	ret = snprintf(gid_str, sizeof(gid_str), "%ld", (long)gid);
	if (ret < 1 || (size_t)ret >= sizeof(gid_str))
		return "?";
	return gid_str;
}

