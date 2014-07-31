
/*
 * XoIP -- telephony toolkit.
 *
 * Copyright (C) <2014>, <vassilux>
 *
 * <Vassili Gontcharov> <v.gontcharov@gmail.com>
 *
 */

/*! \file
 *
 * \brief XOIP types 
 *
 * \author\verbatim <Vassili Gontcharov> <vassili.gontcharov@esifrance.net> \endverbatim
 * 
 *  
 * \ingroup applications
 */

#ifndef  XOIP_TYPES_INC
#define  XOIP_TYPES_INC

#include "stdbool.h"

typedef enum
{
  PROT_NULL = 0,
  PROT_PABX = 1,
  PROT_DATA_ENTRANT = 2,
  PROT_V21 = '1',
  PROT_APTEL = '2',
  PROT_SURTEC = '3',
  PROT_CESA = '4',
  PROT_SERIEE = '5',
  PROT_CESAKONE = '7',
  PROT_KONEXION = '8',
  PROT_AETA = '9',
  PROT_ANEP = 'A',
  PROT_AMPHITECH = 'B',
  PROT_STRATEL = 'C',
  PROT_SCANCOM = 'D',
  PROT_BIOTEL102 = 'E',
  PROT_SIA = 'H',
  PROT_CONTACT_ID = 'I',
  PROT_BOSCH = 'K',
  PROT_HORSIN = 'L',
  PROT_WIT = 'M',
  PROT_ANT = 'N',
  PROT_DAITEM = 'O',
  PROT_PHONIQUE = 'P',
  PROT_TOURRET = 'Q',
  PROT_PLATON = 'R',
  PROT_SIA_Long = 'S',
  PROT_PHO_DEC = 'T',
  PROT_SIA_1S = 'V',
  PROT_STDB = 'W',
  PROT_STMF = 'Y',
  PROT_DATACALL = 'Z',
  PROT_ATENDO = 'b',
  PROT_CORA = 'c',
  PROT_SCANCOM2 = 'd',
  PROT_SECOM3 = 'e',
  PROT_CESA_court = 'x',
  PROT_CESA_synto = 'y',
  PROT_SILENT_KNIGHT = '#',
  PROT_FAST_SILENT_KNIGHT = 'F',
  PROT_ROBOFON = 'r',
  PROT_L400 = 'f',
  PROT_FAST_PULSE = 'h',
  PROT_SCANCOM3 = 'i',
  PROT_TELIM = 'j',
  PROT_FAST_PULSE_1400 = 'k',
  PROT_RB2000E = 'm',
  PROT_SAFELINE = 'n',
  PROT_SCANCOMFA = 'o'
} xoip_protocol_t;

#endif /* -----  not XOIP_TYPES_INC  ----- */
