#pragma once

class ISeqDataProvider {
  public:
    virtual int GetElementsCount(int track) const = 0;
    virtual void GetElementInfo(int track, int element, float time_range[2]) const = 0;
};