/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2013 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "stats-log.h"
#include "stats.h"

static void
stats_log_format_counter(StatsCluster *sc, gint type, StatsCounterItem *item, gpointer user_data)
{
  EVTREC *e = (EVTREC *) user_data;
  EVTTAG *tag;
  gchar buf[32];

  tag = evt_tag_printf(stats_get_tag_name(type), "%s(%s%s%s)=%u", 
                       stats_get_direction_and_source_name(sc->source, buf, sizeof(buf)),
                       sc->id,
                       (sc->id[0] && sc->instance[0]) ? "," : "",
                       sc->instance,
                       stats_counter_get(&sc->counters[type]));
  evt_rec_add_tag(e, tag);
}


void
stats_log_format_cluster(StatsCluster *sc, EVTREC *e)
{
  stats_cluster_foreach_counter(sc, stats_log_format_counter, e);
}
