/*
The contents of this file are subject to the Mozilla Public License Version 1.1 
(the "License"); you may not use this file except in compliance with the License.
You may obtain a copy of the License at http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS" basis, 
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for the 
specific language governing rights and limitations under the License.

The Original Code is ______________________________________.

The Initial Developer of the Original Code is ________________________.
Portions created by ______________________ are Copyright (C) ______ _______________________.
All Rights Reserved.

Contributor(s): ______________________________________.

Alternatively, the contents of this file may be used under the terms of the eCos GPL license 
(the  [eCos GPL] License), in which case the provisions of [eCos GPL] License are applicable 
instead of those above. If you wish to allow use of your version of this file only under the
terms of the [eCos GPL] License and not to allow others to use your version of this file under 
the MPL, indicate your decision by deleting  the provisions above and replace 
them with the notice and other provisions required by the [eCos GPL] License. 
If you do not delete the provisions above, a recipient may use your version of this file under 
either the MPL or the [eCos GPL] License."
*/

//Based on dev_at91sam9261_cpu.h
/*============================================
| Compiler Directive   
==============================================*/
#ifndef _DEV_LM3S_CPU_H
#define _DEV_LM3S_CPU_H


/*============================================
| Includes 
==============================================*/
#if defined(USE_ECOS)
	#include "pkgconf/hal_arm_****.h"
#endif

/*============================================
| Declaration  
==============================================*/
#if defined(USE_SEGGER)
	#define DEV_LM3S_QUARTZ_FREQ  (18432000UL)   //* In Hz  //GD-TODO ?
	#define MUL_PLLA     					 (109)      // Multiplier //for 200 MHz   //GD-TODO
	#define DIV_PLLA     				    (10)       // Divider	
	//from elio board.h: definition for __lowlevel_init compatibility
	#define DEV_LM3S_PLLA_FREQ   ((DEV_LM3S_QUARTZ_FREQ*MUL_PLLA)/DIV_PLLA)
	//	 Master Clock <= 119 MHz
	#define DEV_LM3S_MASTER_CLOCK       (DEV_LM3S_PLLA_FREQ/2)
	
#elif defined(USE_ECOS)
	#define DEV_LM3S_MASTER_CLOCK       (CYGNUM_HAL_ARM_LM3S_CLOCK_SPEED)		
#endif

#endif
