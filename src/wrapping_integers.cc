#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { static_cast<uint32_t>( n ) + zero_point.raw_value_ };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t high_32 = checkpoint & 0xFFFFFFFF00000000;
  uint64_t roundpoint_1 = static_cast<uint64_t>( raw_value_ - zero_point.raw_value_ ) + high_32;
  uint64_t roundpoint_2 = roundpoint_1 <= checkpoint ? roundpoint_1 + 0x100000000 : roundpoint_1 - 0x100000000;
  uint64_t diff_1 = roundpoint_1 > checkpoint ? roundpoint_1 - checkpoint : checkpoint - roundpoint_1;
  uint64_t diff_2 = roundpoint_2 > checkpoint ? roundpoint_2 - checkpoint : checkpoint - roundpoint_2;
  uint64_t ret = diff_1 < diff_2 ? roundpoint_1 : roundpoint_2;

  return ret;
}
