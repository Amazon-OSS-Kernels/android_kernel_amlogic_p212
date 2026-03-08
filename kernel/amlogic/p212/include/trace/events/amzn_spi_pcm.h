#undef TRACE_SYSTEM
#define TRACE_SYSTEM amzn_spi_pcm

#if !defined(_TRACE_AMZN_SPI_PCM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_AMZN_SPI_PCM_H

#include <linux/tracepoint.h>


TRACE_EVENT(spi_pcm_txrx_done,
	TP_PROTO(u32 numframes, unsigned long timediff),
	TP_ARGS(numframes, timediff),

	TP_STRUCT__entry(
	    __field(          u32, numframes    )
	    __field(unsigned long, timediff   )
	   ),

	TP_fast_assign(
	    __entry->numframes = (u32) numframes;
	    __entry->timediff = timediff;
	),

	TP_printk("frames=%u timediff=%lu",
	      __entry->numframes, __entry->timediff)
);

TRACE_EVENT(spi_pcm_txrx_overrun,
	TP_PROTO(unsigned long overrun_duration, unsigned long slept_duration, unsigned long spi_duration),
	TP_ARGS(overrun_duration, slept_duration, spi_duration),

	TP_STRUCT__entry(
	    __field(unsigned long, overrun_duration   )
	    __field(unsigned long, slept_duration   )
	    __field(unsigned long, spi_duration   )
	   ),

	TP_fast_assign(
	    __entry->overrun_duration = overrun_duration;
	    __entry->slept_duration   = slept_duration;
	    __entry->spi_duration     = spi_duration;
	),

	TP_printk("duration: overrun=%u sleep=%lu spi=%lu",
	      __entry->overrun_duration, __entry->slept_duration, __entry->spi_duration)
);

TRACE_EVENT(spi_pcm_txrx_sleep,
	TP_PROTO(unsigned long min_sleep_usec, unsigned long max_sleep_usec),
	TP_ARGS(min_sleep_usec, max_sleep_usec),

	TP_STRUCT__entry(
	    __field(unsigned long, min_sleep_usec )
	    __field(unsigned long, max_sleep_usec )
	   ),

	TP_fast_assign(
	    __entry->min_sleep_usec = min_sleep_usec;
	    __entry->max_sleep_usec = max_sleep_usec;
	),

	TP_printk("min=%lu max=%lu",
	      __entry->min_sleep_usec, __entry->max_sleep_usec)
);



#endif /* _TRACE_AMZN_SPI_PCM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
