#undef TRACE_SYSTEM
#define TRACE_SYSTEM xattr

#if !defined(_TRACE_XATTR_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_XATTR_H

#include <linux/types.h>
#include <linux/tracepoint.h>

TRACE_EVENT(getxattr,
	TP_PROTO(int stage,
		 const char *name,
		 int err,
		 unsigned long arg1,
		 unsigned long arg2),

	TP_ARGS(stage, name, err, arg1, arg2),

	TP_STRUCT__entry(
		__field(	int ,		stage)
		__field(	const char*,	name)
		__field(	int,	err)
		__field(	unsigned long,	arg1)
		__field(	unsigned long,	arg2)
	),

	TP_fast_assign(
		__entry->stage = stage;
		__entry->name  = name;
		__entry->err   = err;
		__entry->arg1  = arg1;
		__entry->arg2  = arg2;
	),

	TP_printk("s:%2d name=%s err=%d arg1=%lx arg2=%lx",
		__entry->stage,
		__entry->name,
		__entry->err,
		__entry->arg1,
		__entry->arg2)
);

#endif
#include <trace/define_trace.h> 
