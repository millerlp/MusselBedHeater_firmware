# MusselBedHeater_firmware
Luke Miller, 2020
 Firmware and hardware for Mussel Bed Heater project that Claire Windecker carried out 
 for her Masters thesis at Cal Poly Humboldt. The project here consists of a circuit board 
 design that uses Arduino software to control an ATmega328P microcontroller to carry out 
 temperature monitoring and control heated artificial mussel beds. 
 
![MusselBedHeater circuit board diagram](./Pictures/MusselBedHeater_board_diagram-01.png)

Hardware_files/ contains parts lists, bill of materials, and Autocad Eagle design files. Revision G is the
most recent version, and contains all of the modifications made to the Revision F boards that were used 
in the field experiment. 




library_copies/ contains copies of the necessary Arduino libraries used to run the MusselBedHeater_firmware

Arduino programs used to run the hardware in the field are contained in:
* MusselBedHeater_6Crise/
* MusselBedHeater_field/

Utility Arduino programs used to set up the boards initially are in:
* serial_number_generator/
* settime_Serial/
