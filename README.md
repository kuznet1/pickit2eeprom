# pickit2eeprom
Programming large EEPROM of 25 series via PICkit2
(25C often uses as BIOS flash)

Copyright (C) 2015 Alexey Kuznetsov

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
or see <http://www.gnu.org/licenses/>

 USAGE:

Read:	-r <file_name> <size>[K|M]
Prog:	-p <file_name>
Erase:	-e

 CONNECTING:
  
PICkit2 Pin   25C Device Pin (DIP/SO8)
(1) VPP ----> 1 nCS
(2) Vdd ----> 8 Vcc, 3 nWP, 7 nHOLD
(3) GND ----> 4 Vss
(4) PGD ----> 2 SO
(5) PGC ----> 6 SCK
(6) AUX ----> 5 SI
