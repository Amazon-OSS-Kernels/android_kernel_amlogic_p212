#undef TRACE_SYSTEM
#define TRACE_SYSTEM spicc

#if !defined(_TRACE_SPICC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SPICC_H

#include <linux/tracepoint.h>


TRACE_EVENT(spicc_transfer,
	TP_PROTO(struct spi_message *msg),
	TP_ARGS(msg),

	TP_STRUCT__entry(
		__field(        struct spi_message *,   msg     )
	   ),

	TP_fast_assign(
		__entry->msg = msg;
	),

	TP_printk("spi_msg %p", (struct spi_message *)__entry->msg)
);


TRACE_EVENT(spicc_dma_one_burst,
	TP_PROTO(int remain),
	TP_ARGS(remain),

	TP_STRUCT__entry(
	    __field(int, remain )
	   ),

	TP_fast_assign(
			__entry->remain = remain;
	),

	TP_printk("remain=%d", __entry->remain)
);

TRACE_EVENT(spicc_dma_xfer_done,
	TP_PROTO(unsigned long ret),
	TP_ARGS(ret),

	TP_STRUCT__entry(
	    __field(unsigned long, ret )
	   ),

	TP_fast_assign(
			__entry->ret = ret;
	),

	TP_printk("rem jiffies=%x", __entry->ret)
);



#endif /* _TRACE_SPICC_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
