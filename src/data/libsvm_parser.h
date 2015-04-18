/*!
 *  Copyright (c) 2015 by Contributors
 * \file libsvm_parser.h
 * \brief iterator parser to parse libsvm format
 * \author Tianqi Chen
 */
#ifndef DMLC_DATA_LIBSVM_PARSER_H_
#define DMLC_DATA_LIBSVM_PARSER_H_

#include <vector>
#include <cstring>
#include <cctype>
#include <dmlc/data.h>
#include <dmlc/omp.h>
#include "./row_block.h"

namespace dmlc {
namespace data {
/*!
 * \brief libsvm parser that parses the input lines
 * and returns rows in input data
 */
class LibSVMParser : public DataIter<RowBlock<size_t> > {
 public:
  explicit LibSVMParser(InputSplit *source,
                        size_t buffer_size,
                        int nthread)
      : nthread_(nthread), max_buffer_size_(buffer_size),
        bytes_read_(0), at_head_(true),
        data_ptr_(0), data_end_(0), source_(source) {
  }
  virtual ~LibSVMParser() {
    delete source_;
  }
  virtual void BeforeFirst(void) {
    CHECK(at_head_) << "cannot call BeforeFirst on LibSVMIter";
  }
  virtual const RowBlock<size_t> &Value(void) const {
    return block_;
  }
  virtual bool Next(void) {
    while (true) {
      while (data_ptr_ < data_end_) {
        data_ptr_ += 1;
        if (data_[data_ptr_ - 1].Size() != 0) {
          block_ = data_[data_ptr_ - 1].GetBlock();
          return true;
        }
      }
      if (!FillData()) break;
      data_ptr_ = 0; data_end_ = data_.size();
    }
    return false;
  }
  inline size_t bytes_read(void) const {
    return bytes_read_;
  }

 protected:
  inline bool FillData() {
    InputSplit::Blob chunk;
    if (!source_->NextChunk(&chunk)) return false;
    int nthread;
    #pragma omp parallel num_threads(nthread_)
    {
      nthread = omp_get_num_threads();
    }
    // reserve space for data
    data_.resize(nthread);
    bytes_read_ += chunk.size;
    CHECK(chunk.size != 0);
    char *head = reinterpret_cast<char*>(chunk.dptr);        
    #pragma omp parallel num_threads(nthread_)
    {
      // threadid
      int tid = omp_get_thread_num();
      size_t nstep = (chunk.size + nthread - 1) / nthread;
      size_t sbegin = std::min(tid * nstep, chunk.size);
      size_t send = std::min((tid + 1) * nstep, chunk.size);
      char *pbegin = BackFindEndLine(head + sbegin, head);
      char *pend;
      if (tid + 1 == nthread) {
        pend = head + send;
      } else {
        pend = BackFindEndLine(head + send, head);
      }
      ParseBlock(pbegin, pend, &data_[tid]);
    }
    data_ptr_ = 0;
    return true;
  }
  /*!
   * \brief parse data into out
   * \param begin beginning of buffer
   * \param end end of buffer
   */
  inline void ParseBlock(char *begin,
                         char *end,
                         RowBlockContainer<size_t> *out) {    
    out->Clear();
    char *p = begin;
    while (p != end) {
      while (isspace(*p) && p != end) ++p;
      if (p == end) break;
      char *head = p;
      while (isdigit(*p) && p != end) ++p;
      if (*p == ':') {
        out->index.push_back(atol(head));
        out->value.push_back(atof(p + 1));        
      } else {
        if (out->label.size() != 0) {
          out->offset.push_back(out->index.size());
        }
        out->label.push_back(atof(head));
      }
      while (!isspace(*p) && p != end) ++p;
    }
    if (out->label.size() != 0) {
      out->offset.push_back(out->index.size());
    }
    CHECK(out->label.size() + 1 == out->offset.size());
  }
  /*!
   * \brief start from bptr, go backward and find first endof line
   * \param bptr end position to go backward
   * \param begin the beginning position of buffer
   * \return position of first endof line going backward
   */
  inline char* BackFindEndLine(char *bptr,
                               char *begin) {
    for (; bptr != begin; --bptr) {
      if (*bptr == '\n' || *bptr == '\r') return bptr;
    }
    return begin;
  }
  
 private:
  // nthread
  int nthread_;
  // maximum buffer size
  size_t max_buffer_size_;
  // number of bytes readed
  size_t bytes_read_;
  // at beginning, at end of stream
  bool at_head_;
  // pointer to begin and end of data
  size_t data_ptr_, data_end_;
  // source split that provides the data
  InputSplit *source_;
  // internal data
  std::vector<RowBlockContainer<size_t> > data_;
  // internal row block
  RowBlock<size_t> block_;
};
}  // namespace data
}  // namespace dmlc
#endif  // DMLC_DATA_LIBSVM_ITER_H_
