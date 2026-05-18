export const kSampleRateHz = 48000;
export const kChannelCount = 2;
export const kBytesPerSample = 4;
export const kChunkSamplesPerChannel = 1024;
export const kChunkBytes =
  kChunkSamplesPerChannel * kChannelCount * kBytesPerSample;
