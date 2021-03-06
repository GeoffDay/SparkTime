/* Spark Time by Brian Ogilvie. 
Fractional timezones by Geoff Day 3 Jan 2015.
Added Rule based DST for those outside Europe or US by Geoff Day 24 June 2015.
added routine to output hours in 1 to 12 format 3 Sept 2015 by Geoff Day. 
Inspired by Arduino Time by Michael Margolis
Copyright (c) 2014 Brian Ogilvie. All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
TODO: Fractional second handling for NTP and millis();
Translation for other languages
*/
#include "SparkTime.h"
SparkTime::SparkTime()
{
    _UDPClient = NULL;
    _timezoneHrs = 9;	
    _timezoneMns = 30;
    _tz = uint32_t(_timezoneHrs*3600UL + _timezoneMns*60UL);	// make an integer version of _timezone
    _useDST = true;
    _useDSTRule = 3;    //0 US, 1 Euro, 2 anywhere else in Northern Hemi. or 3 anywhere else in Southern Hemisphere. 
    _DSTDayStart = 10;  //two digits - first, second, third or last in the month followed by the day. 10 is first Sunday.
    _DSTMonthStart = 3; //0 - Jan, 1 - Feb, 2 - March, 3 - April and so on. 
    _DSTHourChange = 2; //2 am transistion
    _DSTDayEnd = 10;    //first Sunday again
    _DSTMonthEnd = 9;   //9 is October. In Southern Hemi. the end month is really the start!!
    _syncedOnce = false;
    _interval = 60UL * 60UL;
    _localPort = 2390;
}

void SparkTime::begin(UDP * UDPClient, const char * NTPServer) {
  _UDPClient = UDPClient;
  memcpy(_serverName, NTPServer, strlen(NTPServer)+1);
}

void SparkTime::begin(UDP * UDPClient) {
  _UDPClient = UDPClient;
  const char NTPServer[] = "pool.ntp.org";
  memcpy(_serverName, NTPServer, strlen(NTPServer)+1);
}

bool SparkTime::hasSynced() {
  return _syncedOnce;
}

void SparkTime::setUseDST(bool value) {
  _useDST = value;
}

void SparkTime::setUseDSTRule(uint8_t value) {
  _useDSTRule = value;
}

uint8_t SparkTime::hour(uint32_t tnow) {
  uint8_t hTemp = ((tnow+timeZoneDSTOffset(tnow)) % 86400UL)/3600UL;
  return hTemp;
}

uint8_t SparkTime::hourTwelve(uint32_t tnow) {
  uint8_t hTemp = ((tnow+timeZoneDSTOffset(tnow)) % 86400UL)/3600UL;

  if (hTemp > 12) {
      hTemp -= 12;
  }
  
  if (hTemp == 0) {
      hTemp = 12;
  }

  return hTemp;
}

uint8_t SparkTime::minute(uint32_t tnow) {
  return (((tnow+timeZoneDSTOffset(tnow)) % 3600) / 60);
}

uint8_t SparkTime::second(uint32_t tnow) {
  return ((tnow+timeZoneDSTOffset(tnow)) % 60);
}

uint8_t SparkTime::dayOfWeek(uint32_t tnow) {
  uint32_t dayNum = (tnow + timeZoneDSTOffset(tnow)-SPARKTIMEEPOCHSTART)/SPARKTIMESECPERDAY;
//Unix epoch day 0 was a thursday
  return ((dayNum+4)%7);
}

uint8_t SparkTime::day(uint32_t tnow) {
  uint32_t dayNum = (tnow+timeZoneDSTOffset(tnow)-SPARKTIMEBASESTART)/SPARKTIMESECPERDAY;
  uint32_t tempYear = SPARKTIMEBASEYEAR;
  uint8_t tempMonth = 0;
  
  while(dayNum >= YEARSIZE(tempYear)) {
    dayNum -= YEARSIZE(tempYear);
    tempYear++;
  }
  
  while(dayNum >= _monthLength[LEAPYEAR(tempYear)][tempMonth]) {
    dayNum -= _monthLength[LEAPYEAR(tempYear)][tempMonth];
    tempMonth++;
  }
  
  dayNum++; // correct for zero-base
  return (uint8_t)dayNum;
}

uint8_t SparkTime::month(uint32_t tnow) {
  uint32_t dayNum = (tnow+timeZoneDSTOffset(tnow)-SPARKTIMEBASESTART)/SPARKTIMESECPERDAY;
  uint32_t tempYear = SPARKTIMEBASEYEAR;
  uint8_t tempMonth = 0;

  while(dayNum >= YEARSIZE(tempYear)) {
    dayNum -= YEARSIZE(tempYear);
    tempYear++;
  }
  
  while(dayNum >= _monthLength[LEAPYEAR(tempYear)][tempMonth]) {
    dayNum -= _monthLength[LEAPYEAR(tempYear)][tempMonth];
    tempMonth++;
  }
  
  tempMonth++;
  return tempMonth;
}

uint32_t SparkTime::year(uint32_t tnow) {
  uint32_t dayNum = (tnow+timeZoneDSTOffset(tnow)-SPARKTIMEBASESTART)/SPARKTIMESECPERDAY;
  uint32_t tempYear = SPARKTIMEBASEYEAR;

  while(dayNum >= YEARSIZE(tempYear)) {
    dayNum -= YEARSIZE(tempYear);
    tempYear++;
  }
  
  return tempYear;
}

String SparkTime::hourString(uint32_t tnow) {
  return String(_digits[hour(tnow)]);
}

String SparkTime::hour12String(uint32_t tnow) {
  uint8_t tempHour = hour(tnow);
  
  if (tempHour>12) {
      tempHour -= 12;
  }
  
  if (tempHour == 0) {
      tempHour = 12;
  }
  
  return String(_digits[tempHour]);
}

String SparkTime::minuteString(uint32_t tnow) {
  return String(_digits[minute(tnow)]);
}

String SparkTime::secondString(uint32_t tnow) {
  return String(_digits[second(tnow)]);
}

String SparkTime::AMPMString(uint32_t tnow) {
  uint8_t tempHour = hour(tnow);

  if (tempHour<12) {
      return String("AM");
  } else {
      return String("PM");
  }
}

String SparkTime::dayOfWeekShortString(uint32_t tnow) {
  return String(_days_short[dayOfWeek(tnow)]);
}

String SparkTime::dayOfWeekString(uint32_t tnow) {
  return String(_days[dayOfWeek(tnow)]);
}

String SparkTime::dayString(uint32_t tnow) {
  return String(_digits[day(tnow)]);
}

String SparkTime::monthString(uint32_t tnow) {
  return String(_digits[month(tnow)]);
}

String SparkTime::monthNameShortString(uint32_t tnow) {
  return String(_months_short[month(tnow)-1]);
}

String SparkTime::monthNameString(uint32_t tnow) {
  return String(_months[month(tnow)-1]);
}

String SparkTime::yearShortString(uint32_t tnow) {
  uint32_t tempYear = year(tnow)%100;

  if (tempYear<10) {
      return String(_digits[tempYear]);
  } else {
      return String(tempYear);
  }
}

String SparkTime::yearString(uint32_t tnow) {
  return String(year(tnow));
}

String SparkTime::ISODateString(uint32_t tnow) {
  String ISOString;
  
  ISOString += yearString(tnow);
  ISOString += "-";
  ISOString += monthString(tnow);
  ISOString += "-";
  ISOString += dayString(tnow);
  ISOString += "T";
  ISOString += hourString(tnow);
  ISOString += ":";
  ISOString += minuteString(tnow);
  ISOString += ":";
  ISOString += secondString(tnow);

// create separate hours and minutes offset variables
  int32_t offsetHrs = timeZoneDSTOffset(tnow)/3600L;
  int32_t offsetMins = (timeZoneDSTOffset(tnow) - (offsetHrs * 3600)) / 60;

  // Guard against timezone problems
  if (offsetHrs>-24 && offsetHrs<24) {
      if (offsetHrs < 0) {
          ISOString = ISOString + "-" + _digits[-offsetHrs] + _digits[-offsetMins];
      } else {
          ISOString = ISOString + "+" + _digits[offsetHrs] + _digits[offsetMins];
      }
  }
  
  return ISOString;
}

String SparkTime::ISODateUTCString(uint32_t tnow) {
  int32_t savedTimeZoneHrs = _timezoneHrs;
  int32_t savedTimeZoneMns = _timezoneMns;
  bool savedUseDST = _useDST;
  
  _timezoneHrs = 0;
  _timezoneMns = 0;
  _useDST = false;
  String ISOString;
  
  ISOString += yearString(tnow);
  ISOString += "-";
  ISOString += monthString(tnow);
  ISOString += "-";
  ISOString += dayString(tnow);
  ISOString += "T";
  ISOString += hourString(tnow);
  ISOString += ":";
  ISOString += minuteString(tnow);
  ISOString += ":";
  ISOString += secondString(tnow);
  ISOString += "Z";
  
  _timezoneHrs = savedTimeZoneHrs;
  _timezoneMns = savedTimeZoneMns;
  _useDST = savedUseDST;

  return ISOString;
}

uint32_t SparkTime::now() {
    if (!_syncedOnce && !_isSyncing) {
        updateNTPTime();
    }

    if (!_syncedOnce) { // fail!
        return SPARKTIMEBASEYEAR; // Jan 1, 2014
    }
    return nowNoUpdate();
}
uint32_t SparkTime::nowNoUpdate() {
  uint32_t mTime = millis();
  //unsigned long mFracSec = (mTime%1000); // 0 to 999 miliseconds
  //unsigned long nowFrac = _lastSyncNTPFrac + mFracSec*2^22/1000;
  uint32_t nowSec = _lastSyncNTPTime + ((mTime - _lastSyncMillisTime)/1000);

  if (_lastSyncMillisTime>mTime) { // has wrapped
      nowSec = nowSec + SPARKTIMEWRAPSECS;
  }
  
  if (nowSec >= (_lastSyncNTPTime + _interval)) {
      updateNTPTime();
  }
  
  return nowSec;
}

uint32_t SparkTime::nowEpoch() {
  return (now()-SPARKTIMEEPOCHSTART);
}

uint32_t SparkTime::lastNTPTime() {
  return _lastSyncNTPTime;
}

void SparkTime::setNTPInvterval(uint32_t intervalMinutes) {
  const uint32_t fiveMinutes = 5UL * 60UL;
  uint32_t interval = intervalMinutes * 60UL;
  _interval = max(fiveMinutes, interval);
}

void SparkTime::setTimeZoneHrs(int32_t hoursOffset) {
  _timezoneHrs = hoursOffset;
}

void SparkTime::setTimeZoneMns(int32_t minsOffset) {
  _timezoneMns = minsOffset;
}

void SparkTime::setDSTDayStart(uint32_t dayStart) {
  _DSTDayStart = dayStart;
}

void SparkTime::setDSTMonthStart(uint8_t monthStart) {
  _DSTMonthStart = monthStart;
}

void SparkTime::setDSTHourChange(uint8_t hourChange) {
  _DSTHourChange = hourChange;
}

void SparkTime::setDSTDayEnd(uint32_t dayEnd) {
  _DSTDayEnd = dayEnd;
}

void SparkTime::setDSTMonthEnd(uint8_t monthEnd) {
  _DSTMonthEnd = monthEnd;
}

bool SparkTime::isUSDST(uint32_t tnow) {
  // 2am 2nd Sunday in March to 2am 1st Sunday in November
  // can't use offset here
  bool result = false;

  uint32_t dayNum = (tnow+_tz-SPARKTIMEBASESTART)/SPARKTIMESECPERDAY;
  uint32_t tempYear = SPARKTIMEBASEYEAR;
  uint8_t tempMonth = 0;
  uint8_t tempHour = ((tnow+_tz) % 86400UL)/3600UL;

  while(dayNum >= YEARSIZE(tempYear)) {
    dayNum -= YEARSIZE(tempYear);
    tempYear++;
  }
  
  while(dayNum >= _monthLength[LEAPYEAR(tempYear)][tempMonth]) {
    dayNum -= _monthLength[LEAPYEAR(tempYear)][tempMonth];
    tempMonth++;
  }
  
  tempMonth++;
  dayNum++; // correct for zero-base

  if (tempMonth>3 && tempMonth<11) {
      result = true;
  } else if (tempMonth == 3) {
      if ((dayNum == _usDSTStart[tempYear-SPARKTIMEBASEYEAR] && tempHour >=2) ||
          (dayNum > _usDSTStart[tempYear-SPARKTIMEBASEYEAR])) {
          result = true;
      }
  } else if (tempMonth == 11) {
      if (!((dayNum == _usDSTEnd[tempYear-SPARKTIMEBASEYEAR] && tempHour >=2) ||
          (dayNum > _usDSTEnd[tempYear-SPARKTIMEBASEYEAR]))) {
          result = true;
      }
  }
  return result;
}

bool SparkTime::isEuroDST(uint32_t tnow) {
  // 1am last Sunday in March to 1am on last Sunday October
  // can't use offset here
  bool result = false;
  
  uint32_t dayNum = (tnow+_tz-SPARKTIMEBASESTART)/SPARKTIMESECPERDAY; //_tz is _timezone X 3600
  uint32_t tempYear = SPARKTIMEBASEYEAR;
  uint8_t tempMonth = 0;
  uint8_t tempHour = ((tnow+_tz) % 86400UL)/3600UL;
  
  while(dayNum >= YEARSIZE(tempYear)) {
    dayNum -= YEARSIZE(tempYear);
    tempYear++;
  }

  while(dayNum >= _monthLength[LEAPYEAR(tempYear)][tempMonth]) {
    dayNum -= _monthLength[LEAPYEAR(tempYear)][tempMonth];
    tempMonth++;
  }

  tempMonth++;
  dayNum++; // correct for zero-base

  if (tempMonth>3 && tempMonth<10) {
      result = true;
  } else if (tempMonth == 3) {
      if ((dayNum == _EuDSTStart[tempYear-SPARKTIMEBASEYEAR] && tempHour >=1) ||
          (dayNum > _EuDSTStart[tempYear-SPARKTIMEBASEYEAR])) {
          result = true;
      }
  } else if (tempMonth == 10) {
      if (!((dayNum == _EuDSTEnd[tempYear-SPARKTIMEBASEYEAR] && tempHour >=1) ||
          (dayNum > _EuDSTEnd[tempYear-SPARKTIMEBASEYEAR]))) {
          result = true;
      }
  }

  return result;
}


bool SparkTime::isAnyWhereElseDST(uint32_t tnow) {
  // We'll use those start and end days and months to sort out DST    
  // can't use offset here
  
  uint32_t dayNum = (tnow+_tz-SPARKTIMEBASESTART)/SPARKTIMESECPERDAY; //_tz is _timezone X 3600
  uint32_t tempYear = SPARKTIMEBASEYEAR;
  uint8_t tempMonth = 0;
  uint8_t tempHour = ((tnow+_tz) % 86400UL)/3600UL;
  uint32_t smtDOWeek = (dayNum + 4) % 7;                 //Unix epoch day 0 was a thursday
    
  while(dayNum >= YEARSIZE(tempYear)) {
    dayNum -= YEARSIZE(tempYear);
    tempYear++;
  }

  while(dayNum >= _monthLength[LEAPYEAR(tempYear)][tempMonth]) {
    dayNum -= _monthLength[LEAPYEAR(tempYear)][tempMonth];
    tempMonth++;
  }

  tempMonth++;      
  dayNum++;                                   // correct for zero-base. This is 1 through 31
  uint8_t week = (int(dayNum / 7) * 10) + 10; //2 digits - first, second, third or last in month followed by the day.
  
  if (week == 50) {week = 40;}                // 4th or 5th of the month becomes last     
  
  smtDOWeek += week;

  bool result = false;
  if ((tempMonth > _DSTMonthStart) && (tempMonth < _DSTMonthEnd)) {     //northern hemisphere
      result = true;
  } else if ((tempMonth == _DSTMonthStart) && (((smtDOWeek == _DSTDayStart) && (tempHour >=_DSTHourChange)) || (smtDOWeek > _DSTDayStart))) {
          result = true;
  } else if ((tempMonth == _DSTMonthEnd) && (((smtDOWeek == _DSTDayEnd) && (tempHour >=_DSTHourChange)) || (smtDOWeek > _DSTDayEnd))) {
          result = true;
  }
  return result;
}



void SparkTime::updateNTPTime() {
  //digitalWrite(D7,HIGH);
  _isSyncing = true;
  _UDPClient->begin(_localPort);
  memset(_packetBuffer, 0, SPARKTIMENTPSIZE);
  _packetBuffer[0] = 0b11100011; // LI, Version, Mode
  _packetBuffer[1] = 0; // Stratum, or type of clock
  _packetBuffer[2] = 6; // Polling Interval
  _packetBuffer[3] = 0xEC; // Peer Clock Precision
  // 4-11 are zero
  _packetBuffer[12] = 49;
  _packetBuffer[13] = 0x4E;
  _packetBuffer[14] = 49;
  _packetBuffer[15] = 52;
  _UDPClient->beginPacket(_serverName, 123); //NTP requests are to port 123
  _UDPClient->write(_packetBuffer,SPARKTIMENTPSIZE);
  _UDPClient->endPacket();
  //gather the local offset close to the send time
  uint32_t localmsec = millis();
  int32_t retries = 0;
  int32_t bytesrecv = _UDPClient->parsePacket();

  while(bytesrecv == 0 && retries < 1000) {
    bytesrecv = _UDPClient->parsePacket();
    retries++;
  }
  
  if (bytesrecv>0) {
      _UDPClient->read(_packetBuffer,SPARKTIMENTPSIZE);
      // Handle Kiss-of-death
  
      if (_packetBuffer[1]==0) {
          _interval = max(_interval * 2, 24UL*60UL*60UL);
      }
  
      _lastSyncNTPTime = _packetBuffer[40] << 24 | _packetBuffer[41] << 16 | _packetBuffer[42] << 8 | _packetBuffer[43];
      _lastSyncNTPFrac = _packetBuffer[44] << 24 | _packetBuffer[45] << 16 | _packetBuffer[46] << 8 | _packetBuffer[47];
      _lastSyncMillisTime = localmsec;
      _syncedOnce = true;
    }
    
    //digitalWrite(D7,LOW);
    _UDPClient->stop();
    _isSyncing = false;
}

int32_t SparkTime::timeZoneDSTOffset(uint32_t tnow) {
  int32_t result = _timezoneHrs*3600UL + _timezoneMns*60UL;	
  
  if ((_useDST && ((_useDSTRule == 0) && isUSDST(tnow))) ||
      (_useDST && ((_useDSTRule == 1) && isEuroDST(tnow))) ||
      (_useDST && ((_useDSTRule == 2) && isAnyWhereElseDST(tnow)))) {
      result += 3600UL;
  } else if (_useDST && ((_useDSTRule == 3) && !isAnyWhereElseDST(tnow))) {
      result += 3600UL;
  }
  return result;
}
