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
#include "MotorDriver.h"
#include "A4988MotorDriver.h"

namespace freelss
{

MotorDriver * MotorDriver::m_instance = NULL;

MotorDriver::MotorDriver()
{
	// Do nothing
}

MotorDriver::~MotorDriver()
{
	// Do nothing
}

MotorDriver * MotorDriver::getInstance(const int iMode)
{
	if (MotorDriver::m_instance == NULL)
	{
        A4988MotorDriver::initialize();
        MotorDriver::m_instance = new A4988MotorDriver();
    }

	return MotorDriver::m_instance;
}

void MotorDriver::release()
{
	delete MotorDriver::m_instance;
	MotorDriver::m_instance = NULL;
}

} // ns scanner
