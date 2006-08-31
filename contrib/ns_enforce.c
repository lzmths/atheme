/*
 * Copyright (c) 2005-2006 Atheme Development Group
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the NickServ RELEASE/ENFORCE functions.
 *
 * This does nickserv enforcement on all registered nicks. Users who do
 * not identify within 30-60 seconds have their nick changed to Guest<num>.
 * If the ircd or protocol module do not support forced nick changes,
 * they are killed instead.
 * Enforcement of the nick is currently only supported for bahamut, ratbox
 * and charybdis (i.e. making sure they can't change back immediately).
 * Consequently this module is of little use for other ircds.
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"nickserv/enforce",FALSE, _modinit, _moddeinit,
	"$Id$",
	"Atheme Development Group <http://www.atheme.org>"
);

static void ns_cmd_set_enforce(char *origin);
static void ns_help_set_enforce(char *origin);
static void ns_cmd_release(char *origin);
static void ns_help_release(char *origin);
static void reg_check(void *arg);
static void remove_idcheck(void *vuser);

command_t ns_set_enforce = { "ENFORCE", "Enables or disables automatic protection of a nickname.", AC_NONE, ns_cmd_set_enforce }; 
command_t ns_release = { "RELEASE", "Releases a services enforcer.", AC_NONE, ns_cmd_release };

list_t *ns_cmdtree, *ns_set_cmdtree, *ns_helptree, userlist[HASHSIZE];

#if 0
void manage_bots(void *arg)
{
	user_t *u;
	node_t *n, *tn;
	myuser_t *mu;
	metadata_t *md;
	int i = 0;
	
	for (i = 0; i < HASHSIZE; i++) 
	{
		LIST_FOREACH_SAFE(n, tn, userlist[i].head)
		{
			u = (user_t *)n->data;
			if ((mu = myuser_find(u->nick)))
			{
			if (is_internal_client(u))
			{
				if (md = metadata_find(mu, METADATA_USER, "private:enforcer"))
				{
					int x = atoi(md->value);
					if(x < 5)
					{
						x++;
						char buf[32];
						sprintf(buf, "%d", x);
						metadata_add(mu, METADATA_USER, "private:enforcer", buf);
					}
					else
					{
						metadata_delete(mu, METADATA_USER, "private:enforcer");
						quit_sts(u, "timed out svs enforcer");
					}
				}
			}
			}
		}
	}
}
#endif

static void ns_cmd_set_enforce(char *origin)
{
	user_t *u = user_find_named(origin);
	metadata_t *md;
	char *setting = strtok(NULL, " ");

	if (!setting)
	{
		notice(nicksvs.nick, origin, STR_INSUFFICIENT_PARAMS, "ENFORCE");
		notice(nicksvs.nick, origin, "Syntax: SET ENFORCE <on|off>");
		return;
	}

	if (!u || !u->myuser)
	{
		notice(nicksvs.nick, origin, "You are not logged in.");
		return;
	}

	if (strcasecmp(setting, "ON") == 0)
	{
		if (md = metadata_find(u->myuser, METADATA_USER, "private:doenforce"))
		{
			notice(nicksvs.nick, origin, "ENFORCE is already enabled.");
		}
		else
		{
			metadata_add(u->myuser, METADATA_USER, "private:doenforce", "1");
			notice(nicksvs.nick, origin, "ENFORCE is now enabled.");
		}
	}
	else if (strcasecmp(setting, "OFF") == 0)
	{
		if (md = metadata_find(u->myuser, METADATA_USER, "private:doenforce"))
		{
			metadata_delete(u->myuser, METADATA_USER, "private:doenforce");
			notice(nicksvs.nick, origin, "ENFORCE is now disabled.");
		}
		else
		{
			notice(nicksvs.nick, origin, "ENFORCE is already disabled.");
		}
	}
	else
	{
		notice(nicksvs.nick, origin, "Unknown value for ENFORCE. Expected values are ON or OFF.");
	}
}

static void ns_help_set_enforce(char *origin)
{
	notice(nicksvs.nick, origin, "Help for \2ENFORCE\2:");
	notice(nicksvs.nick, origin, "\2ENFORCE\2 allows you to enable protection for");
	notice(nicksvs.nick, origin, "your nickname.");
	notice(nicksvs.nick, origin, " ");
	notice(nicksvs.nick, origin, "This will automatically change the nick of someone");
	notice(nicksvs.nick, origin, "who attempts to use it without identifying in time,");
	notice(nicksvs.nick, origin, "and temporarily block its use, which can be");
	notice(nicksvs.nick, origin, "removed at your discretion. See help on RELEASE.");
	notice(nicksvs.nick, origin, " ");
	notice(nicksvs.nick, origin, "Syntax: SET ENFORCE <ON|OFF>");
}

static void ns_cmd_release(char *origin)
{
	myuser_t *mu;
	node_t *n, *tn;
	metadata_t *md;
	service_t *svs;
	char *target = strtok(NULL, " ");
	char *password = strtok(NULL, " ");
	char *gnick;
	int i;
	user_t *u = user_find_named(target), *m = user_find_named(origin);
	char ign[BUFSIZE];

	if (!target)
	{
		notice(nicksvs.nick, origin, STR_INSUFFICIENT_PARAMS, "RELEASE");
		notice(nicksvs.nick, origin, "Syntax: RELEASE <nick> [password]");
		return;
	}

	mu = myuser_find(target);
	
	if (!mu)
	{
		notice(nicksvs.nick, origin, "\2%s\2 is not a registered nickname.", target);
		return;
	}
	
	if (u == m)
	{
		notice(nicksvs.nick, origin, "You cannot RELEASE yourself.");
		return;
	}
	if ((m->myuser == mu) || verify_password(mu, password))
	{
		if (u == NULL || is_internal_client(u))
		{
			if (md = metadata_find(mu, METADATA_USER, "private:enforcer"))
				metadata_delete(mu, METADATA_USER, "private:enforcer");
			logcommand(nicksvs.me, m, CMDLOG_DO, "RELEASE %s", target);
			holdnick_sts(nicksvs.me->me, 0, target, mu);
			notice(nicksvs.nick, origin, "\2%s\2 has been released.", target);
			/*hook_call_event("user_identify", u);*/
		}
		else
		{
			if (md = metadata_find(mu, METADATA_USER, "private:enforcer"))
				metadata_delete(mu, METADATA_USER, "private:enforcer");
			
			notice(nicksvs.nick, target, "%s!%s@%s has released your nickname.", m->nick, m->user, m->vhost);
			i = 0;
			for (i = 0; i < 30; i++)
			{
				snprintf( ign, BUFSIZE, "Guest%d", rand( )%99999 );
				gnick = ign;
				if (!user_find_named(ign))
					break;
			}
			fnc_sts(nicksvs.me->me, u, gnick, FNC_FORCE);
			notice(nicksvs.nick, origin, "%s has been released.", target);
			logcommand(nicksvs.me, m, CMDLOG_DO, "RELEASE %s", target);
			/*hook_call_event("user_identify", u);*/
		}
		return;
	}
	if (!password)
	{
		notice(nicksvs.nick, origin, STR_INSUFFICIENT_PARAMS, "RELEASE");
		notice(nicksvs.nick, origin, "Syntax: RELEASE <nickname> [password]");
		return;
	}
	else
	{
		notice(nicksvs.nick, origin, "%s password incorrect", target);
		logcommand(nicksvs.me, m, CMDLOG_DO, "failed RELEASE %s (bad password)", target);
	}
}

static void ns_help_release(char *origin)
{
	notice(nicksvs.nick, origin, "Help for \2RELEASE\2:");
	notice(nicksvs.nick, origin, "\2RELEASE\2 removes an enforcer for your nick or");
	notice(nicksvs.nick, origin, "changes the nick of a user that is using your");
	notice(nicksvs.nick, origin, "nick.");
	notice(nicksvs.nick, origin, " ");
	notice(nicksvs.nick, origin, "Enforcers are created when someone uses your");
	notice(nicksvs.nick, origin, "nick without identifying and prevent all use");
	notice(nicksvs.nick, origin, "of it.");
	notice(nicksvs.nick, origin, " ");
	notice(nicksvs.nick, origin, "Not all ircds support removing enforcers. You will");
	notice(nicksvs.nick, origin, "have to wait a few minutes then.");
	notice(nicksvs.nick, origin, " ");
	notice(nicksvs.nick, origin, "Syntax: RELEASE <nick> [password]");
}

void reg_check(void *arg)
{
	user_t *u;
	node_t *n, *tn;
	myuser_t *mu;
	metadata_t *md;
	time_t ts = CURRTIME;
	service_t *svs;
	char *uid, *gnick;
	int i = 0, x = 0;
	char ign[BUFSIZE];
	
	for (i = 0; i < HASHSIZE; i++)
	{
		LIST_FOREACH_SAFE(n, tn, userlist[i].head)
		{
			u = (user_t *)n->data;
			/* nick is a service, ignore it */
			if (is_internal_client(u))
				break;
			else
			{
				if ((mu = myuser_find(u->nick)))
				{
					if (u->myuser == mu)
						continue;
					if (!metadata_find(mu, METADATA_USER, "private:doenforce"))
						;
					else if (u->flags & UF_NICK_WARNED)
					{
						notice(nicksvs.nick, u->nick, "You failed to authenticate in time for the nickname %s", u->nick);
						/* Generate a new guest nickname and check if it already exists
						 * This will try to generate a new nickname 30 different times
						 * if nicks are in use. If it runs into 30 nicks in use, maybe 
						 * you shouldn't use this module. */
						for (x = 0; x < 30; x++)
						{
							snprintf( ign, BUFSIZE, "Guest%d", rand( )%99999 );
							gnick = ign;
							if (!user_find_named(ign))
								break;
						}
						fnc_sts(nicksvs.me->me, u, gnick, FNC_FORCE);
						holdnick_sts(nicksvs.me->me, 3600, u->nick, mu);
#if 0 /* can't do this, need to wait for SVSNICK to complete! -- jilles */
						uid = uid_get();
						introduce_nick(u->nick, "enforcer", "services.hold", "Services Enforcer", uid);
						user_add(u->nick, "enforcer", "services.hold", NULL, NULL, uid, "Services Enforcer", me.me, CURRTIME);
#endif
						u->flags &= ~UF_NICK_WARNED;
						metadata_add(mu, METADATA_USER, "private:enforcer", "1");
					}
					else
					{
						u->flags |= UF_NICK_WARNED;
						notice(nicksvs.nick, u->nick, "You have 30 seconds to identify to your nickname before it is changed.");
					}
				}
				else
					continue;
			}
		}
	}

}

static void remove_idcheck(void *vuser)
{
	user_t *u;

	u = vuser;
	u->flags &= ~UF_NICK_WARNED;
}

static int idcheck_foreach_cb(dictionary_elem_t *delem, void *privdata)
{
	metadata_t *md;
	myuser_t *mu = (myuser_t *) delem->node.data;

	if (md = metadata_find(mu, METADATA_USER, "private:idcheck"))
		metadata_delete(mu, METADATA_USER, "private:idcheck");

	return 0;
}

void _modinit(module_t *m)
{
	node_t *n, *tn;
	myuser_t *mu;
	metadata_t *md;
	int i = 0;
	
	MODULE_USE_SYMBOL(ns_cmdtree, "nickserv/main", "ns_cmdtree");
	MODULE_USE_SYMBOL(ns_helptree, "nickserv/main", "ns_helptree");
	MODULE_USE_SYMBOL(ns_set_cmdtree, "nickserv/set", "ns_set_cmdtree");

	/* Leave this for compatibility with old versions of this code
	 * -- jilles
	 */
	dictionary_foreach(mulist, idcheck_foreach_cb, NULL);

	event_add("reg_check", reg_check, NULL, 30);
	/*event_add("manage_bots", manage_bots, NULL, 30);*/
	command_add(&ns_release, ns_cmdtree);
	command_add(&ns_set_enforce, ns_set_cmdtree);
	help_addentry(ns_helptree, "RELEASE", NULL, ns_help_release);
	help_addentry(ns_helptree, "SET ENFORCE", NULL, ns_help_set_enforce);
	hook_add_event("user_identify");
	hook_add_hook("user_identify", remove_idcheck);
}

void _moddeinit()
{
	node_t *n, *tn;
	myuser_t *mu;
	metadata_t *md;
	int i = 0;
	
	event_delete(reg_check, NULL);
	/*event_delete(manage_bots, NULL);*/
	command_delete(&ns_release, ns_cmdtree);
	command_delete(&ns_set_enforce, ns_set_cmdtree);
	help_delentry(ns_helptree, "RELEASE");
	help_delentry(ns_helptree, "SET ENFORCE");
	hook_del_hook("user_identify", remove_idcheck);
}
