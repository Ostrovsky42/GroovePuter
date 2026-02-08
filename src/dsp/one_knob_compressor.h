#pragma once

class OneKnobCompressor {
public:
  OneKnobCompressor();
  void setAmount(float amount);
  void setMix(float mix);
  void setEnabled(bool on);
  bool isEnabled() const;
  void reset();
  float process(float input);

private:
  float amount_;
  float mix_;
  bool enabled_;
  float envelope_;
};
