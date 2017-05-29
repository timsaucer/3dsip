/*
 ****************************************************************************
 *  Copyright (c) 2014 Uriah Liggett <freelaserscanner@gmail.com>           *
 *	This file is part of FreeLSS.                                           *
 *                                                                          *
 *  FreeLSS is free software: you can redistribute it and/or modify         *
 *  it under the terms of the GNU General Public License as published by    *
 *  the Free Software Foundation, either version 3 of the License, or       *
 *  (at your option) any later version.                                     *
 *                                                                          *
 *  FreeLSS is distributed in the hope that it will be useful,              *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *  GNU General Public License for more details.                            *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with FreeLSS.  If not, see <http://www.gnu.org/licenses/>.       *
 ****************************************************************************
*/

#pragma once

#include "MotorDriver.h"

namespace freelss
{
    
    /**
     * An implementation of the TurnTable class that works
     * with the DRV8825 motor driver.
     */
    class A4988MotorDriver : public MotorDriver
    {
    public:
        A4988MotorDriver();
        ~ A4988MotorDriver();
        
        /** Rotates this amount in radians */
        int rotate(real &ioTheta);
        
        /** Enable/Disable the stepper motor */
        void setMotorEnabled(bool enabled);
        
        /** Initialize the turn table */
        static void initialize();
        
        /** Go to him position */
        void moveToHomePosition ();
        
    private:
        
        /** Move to the next step */
        void step();
        
        bool reachedLimit ();
        
        static int computeMicrostepRatio ();
        
        /** The time to sleep between steps in microseconds */
        int m_stepDelay;
        
        /** The time for the stepper driver to detect a voltage transition */
        int m_responseDelay;
        
        /** The enable pin */
        int m_enablePin;
        
        /** The step pin */
        int m_stepPin;
        
        /** The direction pin */
        int m_directionPin;
        
        /** The number of steps per revolution */
        int m_stepsPerRevolution;
        
        /** The time to sleep between steps in microseconds */
        int m_stabilityDelay;
        
        /** The microstepping pins */
        int m_microstepPins[3];
        
        /** The microstepping ratio, calculated from the micro step factor
         For example, if factor = 3, then ratio = 2^3 = 8, so 8 micro steps */
        int m_microstepRatio;
        
        /** The safety limit switch pin */
        int m_limitSwitchPin;
        
        /** Value of the limit switch when triggered */
        int m_limitSwitchValue;
    };
    
}

