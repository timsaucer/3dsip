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

#include "Main.h"
#include "A4988MotorDriver.h"
#include "Thread.h"
#include "Setup.h"
#include "PresetManager.h"

namespace freelss
{
    
    A4988MotorDriver::A4988MotorDriver()
    {
        Setup * setup = Setup::get();
        m_responseDelay = setup->motorResponseDelay;
        m_stepDelay = setup->motorStepDelay;
        m_enablePin = setup->motorEnablePin;
        m_stepPin = setup->motorStepPin;
        m_directionPin = setup->motorDirPin;
        m_stepsPerRevolution = setup->stepsPerRevolution;
        m_stabilityDelay = PresetManager::get()->getActivePreset().stabilityDelay;
        
        m_microstepPins[0] = setup->motorMicrostepM2Pin;
        m_microstepPins[1] = setup->motorMicrostepM2Pin;
        m_microstepPins[2] = setup->motorMicrostepM2Pin;
        
        m_limitSwitchPin = setup->limitSwitchPin;
        m_limitSwitchValue = setup->limitSwitchValue;
        m_microstepRatio = computeMicrostepRatio();
    }
    
    A4988MotorDriver::~A4988MotorDriver()
    {
        // Disable the stepper
        digitalWrite(m_enablePin, HIGH);
        Thread::usleep(m_responseDelay);
    }
    
    void A4988MotorDriver::initialize()
    {
        Setup * setup = Setup::get();
        int responseDelay = setup->motorResponseDelay;
        int enablePin = setup->motorEnablePin;
        int stepPin = setup->motorStepPin;
        int directionPin = setup->motorDirPin;
        
        // Disable the stepper
        pinMode(enablePin, OUTPUT);
        digitalWrite (enablePin, HIGH);
        
        // Put us in a known state
        pinMode(stepPin, OUTPUT);
        digitalWrite (stepPin, LOW);
        
        pinMode(directionPin, OUTPUT);
        digitalWrite (directionPin, LOW);
        
        int pins[3] = {setup-> motorMicrostepM0Pin,
            setup-> motorMicrostepM1Pin,
            setup-> motorMicrostepM2Pin};
        
        for (int idxMicrostepPin=0; idxMicrostepPin<3; idxMicrostepPin++)
        {
            if (-1 != pins[idxMicrostepPin])
            {
                pinMode(pins[idxMicrostepPin], OUTPUT);
                int outputVal = setup->motorMicrostepFactor & (1 << idxMicrostepPin) ? HIGH : LOW;
                digitalWrite (pins[idxMicrostepPin], outputVal);
            }
        }
        
        pinMode(setup->limitSwitchPin, INPUT);
        pullUpDnControl(setup->limitSwitchPin, PUD_UP);
        
        Thread::usleep(responseDelay);
    }
    
    void A4988MotorDriver::step()
    {
        digitalWrite(m_stepPin, LOW);
        Thread::usleep(m_responseDelay);
        
        digitalWrite(m_stepPin, HIGH);
        Thread::usleep(m_responseDelay);
        
        // Wait the step delay (this is how speed is controlled)
        Thread::usleep(m_stepDelay);
    }
    
    int A4988MotorDriver::rotate(real &ioTheta)
    {
        // Get the percent of a full revolution that theta is and convert that to number of steps
        int numSteps = (ioTheta / (2 * PI)) * (m_stepsPerRevolution * m_microstepRatio);
        int numComplete; // ensure scope for all compilers
        
        // Step the required number of steps
        for (numComplete = 0; numComplete < numSteps && !reachedLimit(); numComplete++)
        {
            step();
        }
        
        // Sleep the stability delay amount
        Thread::usleep(m_stabilityDelay);
        ioTheta = numComplete * 2 * PI / (m_stepsPerRevolution * m_microstepRatio);
        
        return numComplete;
    }
    
    void A4988MotorDriver::setMotorEnabled(bool enabled)
    {
        int value = enabled ? LOW : HIGH;
        
        digitalWrite (m_enablePin, value);
        Thread::usleep(m_responseDelay);
    }
    
    bool A4988MotorDriver::reachedLimit ()
    {
        return (m_limitSwitchValue == digitalRead(m_limitSwitchPin));
    }
    
    void A4988MotorDriver::moveToHomePosition ()
    {
        pinMode(m_directionPin, OUTPUT);
        digitalWrite (m_directionPin, HIGH);
        
        while (! reachedLimit())
        {
            step ();
        }
        
        // Get to the first position with the switch disengaged
        digitalWrite (m_directionPin, LOW);
        while (reachedLimit())
        {
            step ();
        }
        
    }
    
    int A4988MotorDriver::computeMicrostepRatio ()
    {
        Setup * setup = Setup::get();
        
        int retValue = 1;
        for (int ii=0; ii<setup->motorMicrostepFactor; ii++)
            retValue *= 2;
        
        return retValue;
    }
    
}

