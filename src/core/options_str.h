#ifndef OPTION_STR_H
#define OPTION_STR_H

#define LINE_LENGTH					80
#define MIN_DESC_LENGTH				78

//cbfs
#define OPT_STR_HELP				"help,h"
#define OPT_STR_VERSION				"version,v"
#define OPT_STR_STATS				"stats"
#define OPT_STR_MON_INTERV			"interv"
#define OPT_STR_MON_INTERV_DEFVAL	0
#define OPT_STR_PRINT_KCOVER		"print-cover"
#define OPT_STR_CROSS_CHECK			"cross-check"
#ifndef WIN32
#define OPT_STR_TIMEOUT				"time-limit,t"
#define OPT_STR_MEMOUT				"mem-limit,m"
#endif
#define OPT_STR_INPUT_FILE			"input-file"
#define OPT_STR_TARGET				"target,a"
#define OPT_STR_INI_SHARED			"i-shared,s"
#define OPT_STR_INI_SHARED_DEFVAL	0
#define OPT_STR_INI_LOCAL			"i-local,l"
#define OPT_STR_INI_LOCAL2			"i-local2"
#define OPT_STR_INI_LOCAL_DEFVAL	0
#define OPT_STR_SINGLE_INITIAL		"i-single,i"

#define OPT_STR_NO_POR				"prj-all"
#define OPT_STR_MODE				"mode,d"
#define FW_CON_OPTION_STR			"FC"
#define CON_OPTION_STR				"C"
#define FWBW_OPTION_STR				"FB"
#define FW_OPTION_STR				"F"
#define BW_OPTION_STR				"B"
#define OPT_STR_ACCEL_BOUND			"acc-bound"
#define OPT_STR_ACCEL_BOUND_DEFVAL	500
#define OPT_STR_FW_INFO				"fw-info"
#define OPT_STR_PRINT_HASH_INFO		"hash-info"
#define OPT_STR_FW_ORDER			"fw-order"
#define OPT_STR_NOMAIN_INFO			"no-main-info"
#define OPT_STR_NORES_INFO			"no-result-info"
#define OPT_STR_NOTRACE_INFO		"no-trace-info"

#define ORDER_SMALLFIRST_OPTION_STR	"S"
#define ORDER_LARGEFIRST_OPTION_STR	"L"
#define ORDER_RANDOM_OPTION_STR		"R"
#define OPT_STR_FW_ORDER_DEFVAL		ORDER_SMALLFIRST_OPTION_STR

#define OPT_STR_SATBOUND_FW			"fw-sat-bound"
#define OPT_STR_SATBOUND_BW			"sat-bound,k"
#define OPT_STR_SATBOUND_BW_DEFVAL	2

#define OPT_STR_FW_WEIGHT			"fw-weight"
#define OPT_STR_FW_WIDTH			"max_fw_width"
#define OPT_STR_FW_WIDTH_DEFVAL		0
#define OPT_STR_FW_ITS_THRESHOLD	"fw-weight"
#define OPT_STR_FW_THRESHOLD		"fw-threshold"
#define OPT_STR_FW_THRESHOLD_DEFVAL	0

#define OPT_STR_BW_GRAPH			"write-graph"
#define OPT_STR_FW_TREE				"write-fw-tree"
#define OPT_STR_BW_GRAPH_OPT_none	"none"
#define OPT_STR_BW_GRAPH_OPT_TIKZ	"tikz"
#define OPT_STR_BW_GRAPH_OPT_DOTTY	"dot"
#define OPT_STR_BW_GRAPH_DEFVAL		OPT_STR_BW_GRAPH_OPT_none
#define OPT_STR_BW_INFO				"bw-info"
#define OPT_STR_BW_ORDER			"bw-order"
#define OPT_STR_BW_ORDER_DEFVAL		ORDER_SMALLFIRST_OPTION_STR

#define OPT_STR_BW_WAITFORFW		"fw-blocks-bw"

#define OPT_STR_BW_WEIGHT			"bw-weight"
#define OPT_STR_WEIGHT_DEPTH		"D"
#define OPT_STR_WEIGHT_WIDTH		"W"
#define OPT_STR_WEIGHT_DEFVAL		OPT_STR_WEIGHT_WIDTH

#define OPT_STR_WRITE_BW_TRACE		"write-trace"
#define OPT_STR_READ_BW_TRACE		"read-trace"

//ttsgen
#define OPT_STR_OUTPUT_FILE			"output,o"
#define OPT_STR_OUTPUT_FILE_STDOUT	"stdout"
#define OPT_STR_OUTPUT_FILE_DEFVAL	OPT_STR_OUTPUT_FILE_STDOUT
#define OPT_STR_NUM_SHARED			"num-shared,S"
#define OPT_STR_NUM_SHARED_DEFVAL	10
#define OPT_STR_NUM_LOCALS			"num-local,L"
#define OPT_STR_NUM_LOCALS_DEFVAL	10
#define OPT_STR_NUM_TRANS			"num-trans,T"
#define OPT_STR_NUM_TRANS_DEFVAL	50
#define OPT_STR_TRANS_SCALE			"scale,c"
#define OPT_STR_TRANS_SCALE_DEFVAL	0
#define OPT_STR_BCST_RATIO			"b-ratio,b"
#define OPT_STR_BCST_RATIO_DEFVAL	0
#define OPT_STR_HOR_RATIO			"h-ratio,r"
#define OPT_STR_HOR_RATIO_DEFVAL	20
#define OPT_STR_UNSTRUCTURED_SW		"unstructured,u"

//ttstrans
#define OPT_STR_FORMAT				"format,w"
#define OPT_STR_FORMAT_MIST			"MIST"
#define OPT_STR_FORMAT_TIKZ			"TIKZ"
#define OPT_STR_FORMAT_TTS			"TTS"
#define OPT_STR_FORMAT_LOLA			"LOLA"
#define OPT_STR_FORMAT_TINA			"TINA"
#define OPT_STR_FORMAT_CLASSIFY		"CLASSIFY"
#endif