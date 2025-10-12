#pragma once 
#include <iostream>
#include <iomanip>
#include <sstream>
#include <assert.h>
#include <vector>
#include <limits>
#include "utils/robin_hood.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "utils/assert_t.h"
#include <memory>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/assume_abstract.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/unordered_set.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

using utils::log;
using utils::printlog;
using std::endl;

using std::vector;
using std::string;
using std::stringstream;
// using robin_hood::unordered_map;
// using robin_hood::unordered_set;
using std::unordered_map;
using std::unordered_set;


typedef uint32_t str_idx;
typedef uint32_t obj_idx;
typedef uint32_t cost_type;
const obj_idx invalid_obj_idx = std::numeric_limits<obj_idx>::max();



// these should be aligned to JAVA writing funcitons.
// xxx_CNT will tell the number of fields.
enum NodeAttrIdx {RNODE_ID, BEG_TILE_ID, END_TILE_X, END_TILE_Y, NODE_BASE_COST, BEG_TILE_X, BEG_TILE_Y, NODE_LEN, WIRE_ID, NODE_TYPE, IS_NODE_PINBOUNCE, /*<- from JAVA*/ END_TILE_ID, INTENT_CODE, TILE_TYPE, CONSIDERED, LAGUNA, PRESERVED, /* <- assistance */ NODE_ATTR_CNT};
enum EdgeAttrIdx {START_NODE_ID, END_NODE_ID, TILE_X, TILE_Y, /*<- from JAVA*/ EDGE_ATTR_CNT};
enum ConnAttrIdx {HASH_CODE, SRC_NODE_ID, SNK_NODE_ID, X_MIN, X_MAX, Y_MIN, Y_MAX, NET_ID, CENTER_X, CENTER_Y, DOUBLE_HPWL, /*<- from JAVA*/ IS_INDIRECT, /* <- assistant*/ CONN_ATTR_CNT};
enum TileTypes {OTHERS, INT_TILE, LAG_TILE};
enum NodeType {PINFEED_O, PINFEED_I, PINBOUNCE, SUPER_LONG_LINE, LAGUNA_I, WIRE};

typedef unsigned long long u2long;  // overide the u2long in default types.h, since atomicMin only supoort unsigned long long instead of unsigned long

#define INFTY 4294967295
#define MAX_UI 0xffffffffL
#define MAX_UL 0xffffffffffffffffL
#define PRESENT_COST_FACTOR 1
#define HISTORY_COST_FACTOR 1
#define NUM_THREAD_PER_BLOCK 512
#define NUM_NODES_PER_THREAD 8

#define MAX_NODE_NUM 30000000
#define MAX_EDGE_NUM 130000000
#define MAX_CONN_NUM 10000000 // Hard code. A large value and cannot be smaller than the #connections in all cases
#define MAX_NET_NUM 1000000 // Hard code.

// IntentCode/WireType >>>>>>>>>> 
#define INTENT_DEFAULT 0
#define NODE_OUTPUT 1
#define NODE_DEDICATED 2
#define NODE_GLOBAL_VDISTR 3
#define NODE_GLOBAL_HROUTE 4
#define NODE_GLOBAL_HDISTR 5
#define NODE_PINFEED 6
#define NODE_PINBOUNCE 7
#define NODE_LOCAL 8
#define NODE_HLONG 9
#define NODE_SINGLE 10
#define NODE_DOUBLE 11
#define NODE_HQUAD 12
#define NODE_VLONG 13
#define NODE_VQUAD 14
#define NODE_OPTDELAY 15
#define NODE_GLOBAL_VROUTE 16
#define NODE_GLOBAL_LEAF 17
#define NODE_GLOBAL_BUFG 18
#define NODE_LAGUNA_DATA 19
#define NODE_CLE_OUTPUT 20
#define NODE_INT_INTERFACE 21
#define NODE_LAGUNA_OUTPUT 22
#define GENERIC 23
#define DOUBLE 24
// #define INPUT 25
#define BENTQUAD 26
#define SLOWSINGLE 27
#define CLKPIN 28
// #define GLOBAL 29
// #define OUTPUT 30
#define PINFEED 31
#define BOUNCEIN 32
#define LUTINPUT 33
#define IOBOUTPUT 34
#define BOUNCEACROSS 35
#define VLONG 36
#define OUTBOUND 37
#define HLONG 38
// #define PINBOUNCE 39
#define BUFGROUT 40
#define PINFEEDR 41
#define OPTDELAY 42
#define IOBIN2OUT 43
#define HQUAD 44
#define IOBINPUT 45
#define PADINPUT 46
#define PADOUTPUT 47
#define VLONG12 48
#define HVCCGNDOUT 49
#define SVLONG 50
#define VQUAD 51
#define SINGLE 52
#define BUFINP2OUT 53
#define REFCLK 54
#define NODE_INTF4 55
#define NODE_INTF2 56
#define NODE_CLE_BNODE 57
#define NODE_CLE_CNODE 58
#define NODE_CLE_CTRL 59
#define NODE_HLONG10 60
#define NODE_HLONG6 61
#define NODE_VLONG12 62
#define NODE_VLONG7 63
#define NODE_SDQNODE 64
#define NODE_IMUX 65
#define NODE_INODE 66
#define NODE_HSINGLE 67
#define NODE_HDOUBLE 68
#define NODE_VSINGLE 69
#define NODE_VDOUBLE 70
#define NODE_INTF_BNODE 71
#define NODE_INTF_CNODE 72
#define NODE_INTF_CTRL 73
#define NODE_IRI 74
#define NODE_OPTDELAY_MUX 75
#define NODE_CLE_LNODE 76
#define NODE_GLOBAL_VDISTR_LVL2 77
#define NODE_GLOBAL_VDISTR_LVL1 78
#define NODE_GLOBAL_GCLK 79
#define NODE_GLOBAL_HROUTE_HSR 80
#define NODE_GLOBAL_HDISTR_HSR 81
#define NODE_GLOBAL_HDISTR_LOCAL 82
#define NODE_SLL_INPUT 83
#define NODE_SLL_OUTPUT 84
#define NODE_SLL_DATA 85
#define NODE_GLOBAL_VDISTR_LVL3 86
#define NODE_GLOBAL_VDISTR_LVL21 87
#define NODE_GLOBAL_VDISTR_SHARED 88
// IntentCode/WireType <<<<<<<<<<<

// TileTypeEnum >>>>>>>>>>
#define INT_IBRK_FSR2IO 0
#define INT_IBRK_IO_TERM_B 1
#define RCLK_BRAM_INTF_TD_R 2
#define RCLK_INTF_L_IBRK_IO_L 3
#define INT_INTF_R_TERM_GT_TERM_B 4
#define RCLK_INTF_R_TERM_GT 5
#define CFRM_T 6
#define CMAC_PCIE4_RBRK_FT 7
#define CFRM_CBRK_L_TERM_B 8
#define RCLK_BRAM_INTF_L 9
#define RCLK_INT_L 10
#define CFG_M12BUF_IO_CFG_TOP_L_FT 11
#define CFGIO_FILL_FT 12
#define RCLK_DSP_INTF_R 13
#define CFG_M12BUF_CTR_RIGHT_CFG_OLY_TOP_L_FT 14
#define INT_INTF_R_RBRK 15
#define INT_INTF_L_TERM_T 16
#define INT_INTF_L_TERM_GT 17
#define ILKN_ILKN_FT 18
#define RCLK_CLEM_R 19
#define AMS_M12BUF_SYSMON_BOT_L_FT 20
#define DSP_RBRK 21
#define RCLK_LAG_L 22
#define GTY_L_TERM_B 23
#define CFGIO_CONFIG_RBRK 24
#define CLEM 25
#define INT_INTF_L 26
#define CFG_M12BUF 27
#define LAG_LAG 28
#define CFG_M12BUF_CTR_RIGHT_CFG_OLY_BOT_L_FT 29
#define INT_INTF_L_RBRK 30
#define INT_INTF_L_IO_TERM_T 31
#define PCIE4_TERM_B_FT 32
#define BRAM_TERM_B 33
#define CFG_CFG_FILL_OLY_FT 34
#define NULL_TILE 35 // -> to avoid overriding NULL
#define INT_INTF_L_PCIE4_RBRK 36
#define INT_TERM_B 37
#define CLE_CLE_M_DECAP_RBRK_FT 38
#define INT_INTF_L_PCIE4_TERM_B 39
#define ILKN_CMAC_RBRK_FT 40
#define INT_INTF_L_IO_TERM_B 41
#define LAG_C2L_RBRK 42
#define INT_INTF_L_PCIE4_TERM_T 43
#define CFG_M12BUF_TERM_B_R 44
#define CLE_L_R_RBRK 45
#define CLEM_TERM_B 46
#define CFRM_CBRK_R_TERM_B 47
#define INT_IBRK_FSR2IO_RBRK 48
#define CFG_CONFIG_PCIE4 49
#define CMAC_CMAC_TERM_T_FT 50
#define GTY_L_RBRK 51
#define GTY_R_TERM_B 52
#define CFG_M12BUF_CTR_RIGHT_FT 53
#define XIPHY_L_TERM_B 54
#define ILKN_AMS_RBRK_FT 55
#define INT 56
#define XIPHY_BYTE_L 57
#define CFG_M12BUF_TERM_B_L 58
#define RCLK_RCLK_XIPHY_INNER_FT 59
#define AMS_AMS_FILL_FT 60
#define CLEM_RBRK 61
#define CFRM_CBRK_L 62
#define CFRM_CBRK_IO_L 63
#define RCLK_CBRK_M12BUF_R 64
#define URAM_URAM_RBRK_FT 65
#define PCIE4_PCIE4_FT 66
#define DSP_TERM_T 67
#define CFG_M12BUF_TERM_T_R 68
#define RCLK_CLEM_L 69
#define CMAC 70
#define AMS_M12BUF_AMS_TOP_L_FT 71
#define URAM_URAM_TERM_B_FT 72
#define RCLK_CLEL_R_L 73
#define CFRM_AMS_CFGIO 74
#define HPIO_L_TERM_T 75
#define INT_INTF_L_TERM_G_TERM_T 76
#define XIPHY_L_TERM_T 77
#define AMS_M12BUF_AMS_BOT_L_FT 78
#define GTY_R_RBRK 79
#define CFRM_CBRK_CTR_RIGHT_L_FT 80
#define RCLK_INTF_L_IBRK_PCIE4_L 81
#define CLEM_R 82
#define CFRM_CBRK_L_RBRK 83
#define RCLK_RCLK_CBRK_CTR_RIGHT_M12BUF_L_FT 84
#define RCLK_INTF_L_TERM_GT 85
#define AMS_M12BUF_IO_AMS_TOP_L_FT 86
#define HPIO_L_RBRK 87
#define INT_INTF_L_PCIE4 88
#define RCLK_RCLK_CTR_FILL_FT 89
#define RCLK_IBRK_IO 90
#define AMS_M12BUF_SYSMON_TOP_L_FT 91
#define AMS 92
#define INT_IBRK_IO_TERM_T 93
#define XIPHY_L_RBRK 94
#define INT_INTF_R 95
#define INT_RBRK 96
#define RCLK_INT_R 97
#define RCLK_AMS_CFGIO 98
#define CMT_L 99
#define CFG_M12BUF_CFG_BOT_R_FT 100
#define CMT_L_RBRK 101
#define RCLK_CLEL_R_R 102
#define CFG_M12BUF_CFG_TOP_R_FT 103
#define RCLK_DSP_INTF_L 104
#define RCLK_IBRK_FSR2IO 105
#define URAM_URAM_DELAY_FT 106
#define INT_IBRK_IO 107
#define CFG_M12BUF_CFG_BOT_L_FT 108
#define CLEL_R_TERM_B 109
#define INT_IBRK_FSR2IO_TERM_T 110
#define INT_INTF_R_TERM_GT 111
#define URAM_URAM_FT 112
#define INT_INTF_L_IO 113
#define GTY_L 114
#define LAG_LAG2C_RBRK 115
#define RCLK_CLEM_CLKBUF_L 116
#define INT_INTF_L_TERM_GT_RBRK 117
#define RCLK_DSP_INTF_CLKBUF_L 118
#define INT_INTF_R_TERM_GT_TERM_T 119
#define CFRM_T_RBRK 120
#define CFG_M12BUF_IO_L_FT 121
#define CFRM_CONFIG 122
#define ILKN_ILKN_RBRK_FT 123
#define CFG_M12BUF_RBRK_L 124
#define RCLK_BRAM_INTF_TD_L 125
#define INT_INTF_R_PCIE4_TERM_T 126
#define CFRM_CBRK_R 127
#define INT_INTF_R_PCIE4_RBRK 128
#define CFG_M12BUF_CTR_LEFT_FT 129
#define CFG_M12BUF_TERM_T_L 130
#define CLEM_TERM_T 131
#define CFGIO_IOB20 132
#define RCLK_RCLK_CLE_M_DECAP_L_FT 133
#define CMT_L_TERM_T 134
#define URAM_URAM_TERM_T_FT 135
#define INT_INTF_R_PCIE4 136
#define PCIE4_CMAC_RBRK_FT 137
#define INT_IBRK_FSR2IO_TERM_B 138
#define CFRM_CBRK_CTR_LEFT_L_FT 139
#define LAG_LAG_TERM_B 140
#define INT_INTF_L_TERM_B 141
#define INT_IBRK_IO_RBRK 142
#define BRAM_TERM_T 143
#define GTY_L_TERM_T 144
#define CFG_M12BUF_RBRK_R 145
#define INT_TERM_T 146
#define CMAC_TERM_B 147
#define RCLK_INTF_L_IBRK_PCIE4_R 148
#define RCLK_CBRK_M12BUF_L 149
#define RCLK_RCLK_CBRK_IO_M12BUF_L_FT 150
#define CMT_L_TERM_B 151
#define RCLK_RCLK_CLE_M_DECAP_R_FT 152
#define GTY_R 153
#define RCLK_INTF_R_IBRK_L 154
#define ILKN_AMS_FILL_RBRK_FT 155
#define RCLK_RCLK_URAM_INTF_L_FT 156
#define AMS_M12BUF_IO_AMS_BOT_L_FT 157
#define CFG_CONFIG 158
#define CLEL_R_TERM_T 159
#define INT_INTF_L_CMT 160
#define LAG_LAG_TERM_T 161
#define AMS_M12BUF_AMS_BOT_R_FT 162
#define AMS_M12BUF_CTR_RIGHT_AMS_ALTO_BOT_L_FT 163
#define RCLK_HPIO_L 164
#define INT_INTF_L_IO_RBRK 165
#define CLE_CLE_M_DECAP_FT 166
#define HPIO_L_TERM_B 167
#define INT_INTF_R_TERM_GT_RBRK 168
#define CMAC_ILKN_RBRK_FT 169
#define DSP_TERM_B 170
#define ILKN_ILKN_CFG_TERM_T_FT 171
#define HPIO_L 172
#define INT_INTF_R_PCIE4_TERM_B 173
#define CFRM_TERM_B 174
#define CFG_M12BUF_IO_CFG_BOT_L_FT 175
#define BRAM 176
#define CFG_CFG_FILL_RBRK_FT 177
#define CFG_M12BUF_CFG_TOP_L_FT 178
#define RCLK_LAG_R 179
#define CFRM_CBRK_R_TERM_T 180
#define CFG_M12BUF_TERM_L 181
#define CFRM_CBRK_L_TERM_T 182
#define CFRM_B 183
#define INT_INTF_L_TERM_G_TERM_B 184
#define CFRM_CBRK_R_RBRK 185
#define DSP 186
#define AMS_M12BUF_CTR_RIGHT_AMS_ALTO_TOP_L_FT 187
#define INT_INTF_R_TERM_T 188
#define BRAM_RBRK 189
#define GTY_R_TERM_T 190
#define CFGIO_CFG_FILL_RBRK_FT 191
#define INT_INTF_R_TERM_B 192
#define CFRM_B_RBRK 193
#define AMS_M12BUF_AMS_TOP_R_FT 194
#define CLEL_R 195
#define CFRM_TERM_T 196
// TileTypeEnum <<<<<<<<<<