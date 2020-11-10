/*

  Threads.h - Threads library
  (c)2019 Iacopo Baroncini

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  
  16 Oct 2019 - initial release 

*/


#ifndef _H_THREADS
#define _H_THREADS


#include <Arduino.h>


#define MAX_NUM_OF_THREADS      4


typedef enum {    
  T_STACK_64,
  T_STACK_128,
  T_STACK_256,
  T_STACK_512
} ThreadStackSize;


class Thread {
    
    private:
    
        int8_t id;
    
    protected:
        
        void yield();
        void yieldAndDelay(uint32_t ms);
        bool periodSync(uint32_t ms);
        uint32_t periodDelay();
        void suspend();
        void terminateAll();
    
    public:
        
        Thread(ThreadStackSize stackSize);
        uint8_t getID();
        void resume();
        void terminate();

        virtual void setup() {};
        virtual void body() = 0;
        virtual void wrapup() {}

};


bool runAllThreads();


#define runEvery(x) while (periodSync(x))
#define infiniteLoop while (true)


#endif