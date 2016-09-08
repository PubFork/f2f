#pragma once

namespace f2f { namespace util 
{

template<class ForwardIt1, class ForwardIt2, class OutputIt>
void InsertAndCopyBackward(
  ForwardIt1 first, ForwardIt1 last,
  ForwardIt1 insert_position,
  ForwardIt2 insert_first, ForwardIt2 insert_last,
  OutputIt output_last)
{
  for (; first != last || insert_first != insert_last; )
  {
    if (last == insert_position && insert_first != insert_last)
      *(--output_last) = *(--insert_last);
    else
      *(--output_last) = *(--last);
  }
}

template<typename OutputIterator>
class SplitOutputIterator
{
public:
  SplitOutputIterator(OutputIterator range1_first, OutputIterator range1_last, OutputIterator range2_first)
    : range1_first(range1_first)
    , range1_last (range1_last)
    , range2_first(range2_first)
  {}

  void operator= (typename std::iterator_traits<OutputIterator>::reference value)
  {
    if (range1_first == range1_last)
      *(range2_first++) = value;
    else
      *(range1_first++) = value;
  }

  SplitOutputIterator & operator * () { return *this; }
  SplitOutputIterator & operator --() { return *this; }

private:
  OutputIterator range1_first, range1_last, range2_first;
};

template<typename OutputIterator>
SplitOutputIterator<OutputIterator> MakeSplitOutputIterator(
  OutputIterator range1_first, 
  OutputIterator range1_last, 
  OutputIterator range2_first)
{
  return SplitOutputIterator<OutputIterator>(range1_first, range1_last, range2_first);
}

}}