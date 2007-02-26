/*
 * Copyright (c) 2006 Jilles Tjoelker
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Module to disable halfop (+h) mode.
 * This will stop Atheme setting this mode by itself, but it can still
 * be used via OperServ MODE etc.
 *
 * Note: this module does not work with the halfops autodetection
 * in the charybdis protocol module.
 *
 * $Id: ircd_nohalfops.c 7753 2007-02-26 15:28:07Z jilles $
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"ircd_nohalfops", FALSE, _modinit, _moddeinit,
	"$Id: ircd_nohalfops.c 7753 2007-02-26 15:28:07Z jilles $",
	"Atheme Development Group <http://www.atheme.org>"
);

boolean_t oldflag;

void _modinit(module_t *m)
{

	if (ircd == NULL)
	{
		slog(LG_ERROR, "Module %s must be loaded after a protocol module.", m->name);
		m->mflags = MODTYPE_FAIL;
		return;
	}
	oldflag = ircd->uses_halfops;
	ircd->uses_halfops = FALSE;
	update_chanacs_flags();
}

void _moddeinit()
{

	ircd->uses_halfops = oldflag;
	update_chanacs_flags();
}
