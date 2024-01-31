/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_en50221_trace

#if !defined(_TRACE_EN50221_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EN50221_H

#include <linux/tracepoint.h>
//trace_en50221_read_data("%s %d in loop cost:%lld\n",
//__func__, __LINE__, ktime_to_us(ktime_sub(ktime_get(), ci_stime6)));
TRACE_EVENT(en50221_read_data_in,
		TP_PROTO(int firstarg, s64 secondarg),

		TP_ARGS(firstarg, secondarg),

		TP_STRUCT__entry(
			__field(int, line)
			__field(s64, cost)
		),

		TP_fast_assign(
			__entry->line = firstarg;
			__entry->cost = secondarg;
		),

		TP_printk("%d loop cost:%lld",
			__entry->line,
			__entry->cost)
);

TRACE_EVENT(en50221_read_data_interrupt,
		TP_PROTO(int firstarg, int secondarg, s64 thirdarg),

		TP_ARGS(firstarg, secondarg, thirdarg),

		TP_STRUCT__entry(
			__field(int, line)
			__field(int, status)
			__field(s64, cost)
		),

		TP_fast_assign(
			__entry->line = firstarg;
			__entry->status = secondarg;
			__entry->cost = thirdarg;
		),

		TP_printk("%d wake_up_interruptible recv%d da to last cost:%lld",
			__entry->line,
			__entry->status,
			__entry->cost)
);

TRACE_EVENT(en50221_read_data_out,
		TP_PROTO(int firstarg, int secondarg, u8 thirdarg, s64 fortharg),

		TP_ARGS(firstarg, secondarg, thirdarg, fortharg),

		TP_STRUCT__entry(
			__field(int, line)
			__field(int, status)
			__field(u8, data)
			__field(s64, cost)
		),

		TP_fast_assign(
			__entry->line = firstarg;
			__entry->status = secondarg;
			__entry->data = thirdarg;
			__entry->cost = fortharg;
		),

		TP_printk("%d recv=%d buf[2]=%02x cost:%lld",
			__entry->line,
			__entry->status,
			__entry->data,
			__entry->cost)
);

TRACE_EVENT(en50221_write_data_in,
		TP_PROTO(int firstarg),

		TP_ARGS(firstarg),

		TP_STRUCT__entry(
			__field(int, line)
		),

		TP_fast_assign(
			__entry->line = firstarg;
		),

		TP_printk("%d",
			__entry->line)
);

TRACE_EVENT(en50221_write_data_out,
		TP_PROTO(int firstarg, s64 secondarg),

		TP_ARGS(firstarg, secondarg),

		TP_STRUCT__entry(
			__field(int, line)
			__field(s64, cost)
		),

		TP_fast_assign(
			__entry->line = firstarg;
			__entry->cost = secondarg;
		),

		TP_printk("%d cost:%lld",
			__entry->line,
			__entry->cost)
);

TRACE_EVENT(en50221_slot_shutdown_in,
		TP_PROTO(int firstarg),

		TP_ARGS(firstarg),

		TP_STRUCT__entry(
			__field(int, line)
		),

		TP_fast_assign(
			__entry->line = firstarg;
		),

		TP_printk("%d wake_up_interruptible",
			__entry->line)
);

TRACE_EVENT(en50221_thread_in,
		TP_PROTO(int firstarg, s64 secondarg),

		TP_ARGS(firstarg, secondarg),

		TP_STRUCT__entry(
			__field(int, line)
			__field(s64, cost)
		),

		TP_fast_assign(
			__entry->line = firstarg;
			__entry->cost = secondarg;
		),

		TP_printk("%d loop cost %lld",
			__entry->line,
			__entry->cost)
);

TRACE_EVENT(en50221_thread_out,
		TP_PROTO(int firstarg, s64 secondarg),

		TP_ARGS(firstarg, secondarg),

		TP_STRUCT__entry(
			__field(int, line)
			__field(s64, cost)
		),

		TP_fast_assign(
			__entry->line = firstarg;
			__entry->cost = secondarg;
		),

		TP_printk("%d cost:%lld",
			__entry->line,
			__entry->cost)
);

TRACE_EVENT(en50221_io_write_in,
		TP_PROTO(int firstarg, int secondarg, s64 thirdarg),

		TP_ARGS(firstarg, secondarg, thirdarg),

		TP_STRUCT__entry(
			__field(int, line)
			__field(int, status)
			__field(s64, cost)
		),

		TP_fast_assign(
			__entry->line = firstarg;
			__entry->status = secondarg;
			__entry->cost = thirdarg;
		),

		TP_printk("%d last_io_read_size=%d read_to_write cost:%lld",
			__entry->line,
			__entry->status,
			__entry->cost)
);

TRACE_EVENT(en50221_io_write_out,
		TP_PROTO(int firstarg, int secondarg, s64 thirdarg),

		TP_ARGS(firstarg, secondarg, thirdarg),

		TP_STRUCT__entry(
			__field(int, line)
			__field(int, status)
			__field(s64, cost)
		),

		TP_fast_assign(
			__entry->line = firstarg;
			__entry->status = secondarg;
			__entry->cost = thirdarg;
		),

		TP_printk("%d send=%d cost:%lld",
			__entry->line,
			__entry->status,
			__entry->cost)
);

TRACE_EVENT(en50221_io_read_in,
		TP_PROTO(int firstarg, s64 secondarg),

		TP_ARGS(firstarg, secondarg),

		TP_STRUCT__entry(
			__field(int, line)
			__field(s64, cost)
		),

		TP_fast_assign(
			__entry->line = firstarg;
			__entry->cost = secondarg;
		),

		TP_printk("%d poll to read cost:%lld",
			__entry->line,
			__entry->cost)
);

TRACE_EVENT(en50221_io_read_out,
		TP_PROTO(int firstarg, int secondarg, s64 thirdarg),

		TP_ARGS(firstarg, secondarg, thirdarg),

		TP_STRUCT__entry(
			__field(int, line)
			__field(int, status)
			__field(s64, cost)
		),

		TP_fast_assign(
			__entry->line = firstarg;
			__entry->status = secondarg;
			__entry->cost = thirdarg;
		),

		TP_printk("%d rev%d cost:%lld",
			__entry->line,
			__entry->status,
			__entry->cost)
);

TRACE_EVENT(en50221_io_poll_in,
		TP_PROTO(int firstarg, s64 secondarg, s64 thirdarg),

		TP_ARGS(firstarg, secondarg, thirdarg),

		TP_STRUCT__entry(
			__field(int, line)
			__field(s64, status)
			__field(s64, cost)
		),

		TP_fast_assign(
			__entry->line = firstarg;
			__entry->status = secondarg;
			__entry->cost = thirdarg;
		),

		TP_printk("%d write to poll cost:%lld us loop cost:%lld",
			__entry->line,
			__entry->status,
			__entry->cost)
);

TRACE_EVENT(en50221_io_poll_out,
		TP_PROTO(int firstarg, int secondarg, s64 thirdarg),

		TP_ARGS(firstarg, secondarg, thirdarg),

		TP_STRUCT__entry(
			__field(int, line)
			__field(int, status)
			__field(s64, cost)
		),

		TP_fast_assign(
			__entry->line = firstarg;
			__entry->status = secondarg;
			__entry->cost = thirdarg;
		),

		TP_printk("%d mask=%x cost:%lld",
			__entry->line,
			__entry->status,
			__entry->cost)
);
#endif /* _TRACE_SUBSYS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
