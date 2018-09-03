// Copyright 2018, Bosch Software Innovations GmbH.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ROSBAG2_STORAGE_DEFAULT_PLUGINS__SQLITE__SQLITE_STATEMENT_WRAPPER_HPP_
#define ROSBAG2_STORAGE_DEFAULT_PLUGINS__SQLITE__SQLITE_STATEMENT_WRAPPER_HPP_

#include <sqlite3.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rosbag2_storage/serialized_bag_message.hpp"
#include "sqlite_exception.hpp"

namespace rosbag2_storage_plugins
{

class SqliteStatementWrapper : public std::enable_shared_from_this<SqliteStatementWrapper>
{
public:
  SqliteStatementWrapper(sqlite3 * database, std::string query);
  SqliteStatementWrapper(const SqliteStatementWrapper &) = delete;
  SqliteStatementWrapper & operator=(const SqliteStatementWrapper &) = delete;
  ~SqliteStatementWrapper();

  template<typename ... Columns>
  class QueryResult
  {
public:
    using RowType = std::tuple<Columns ...>;
    class Iterator : public std::iterator<std::input_iterator_tag, RowType>
    {
public:
      static const int POSITION_END = -1;
      Iterator(std::shared_ptr<SqliteStatementWrapper> statement, int position)
      : statement_(statement), next_row_idx_(position)
      {
        if (next_row_idx_ != POSITION_END) {
          if (statement_->step()) {
            ++next_row_idx_;
          } else {
            next_row_idx_ = POSITION_END;
          }
        }
      }

      Iterator & operator++()
      {
        if (next_row_idx_ != POSITION_END) {
          if (statement_->step()) {
            ++next_row_idx_;
          } else {
            next_row_idx_ = POSITION_END;
          }
          return *this;
        } else {
          throw SqliteException("Cannot increment result iterator beyond result set!");
        }
      }
      Iterator operator++(int)
      {
        auto old_value = *this;
        ++(*this);
        return old_value;
      }

      RowType operator*() const
      {
        RowType row{};
        obtain_row_values(row);
        return row;
      }

      bool operator==(Iterator other) const
      {
        return statement_ == other.statement_ && next_row_idx_ == other.next_row_idx_;
      }
      bool operator!=(Iterator other) const
      {
        return !(*this == other);
      }

private:
      template<typename Indices = std::index_sequence_for<Columns ...>>
      void obtain_row_values(RowType & row) const
      {
        obtain_row_values_impl(row, Indices{});
      }

      template<size_t I, size_t ... Is, typename RemainingIndices = std::index_sequence<Is ...>>
      void obtain_row_values_impl(RowType & row, std::index_sequence<I, Is ...>) const
      {
        statement_->obtain_column_value(I, std::get<I>(row));
        obtain_row_values_impl(row, RemainingIndices{});
      }
      void obtain_row_values_impl(RowType &, std::index_sequence<>) const {}  // end of recursion

      std::shared_ptr<SqliteStatementWrapper> statement_;
      int next_row_idx_;
    };

    explicit QueryResult(std::shared_ptr<SqliteStatementWrapper> statement)
    : statement_(statement)
    {}

    Iterator begin()
    {
      return Iterator(statement_, 0);
    }
    Iterator end()
    {
      return Iterator(statement_, Iterator::POSITION_END);
    }

private:
    std::shared_ptr<SqliteStatementWrapper> statement_;
  };

  std::shared_ptr<SqliteStatementWrapper> execute_and_reset();
  template<typename ... Columns>
  QueryResult<Columns ...> execute_query();

  template<typename T1, typename T2, typename ... Params>
  std::shared_ptr<SqliteStatementWrapper> bind(T1 value1, T2 value2, Params ... values);
  std::shared_ptr<SqliteStatementWrapper> bind(int value);
  std::shared_ptr<SqliteStatementWrapper> bind(rcutils_time_point_value_t value);
  std::shared_ptr<SqliteStatementWrapper> bind(double value);
  std::shared_ptr<SqliteStatementWrapper> bind(std::string value);
  std::shared_ptr<SqliteStatementWrapper> bind(std::shared_ptr<rcutils_char_array_t> value);

  std::shared_ptr<SqliteStatementWrapper> reset();

private:
  bool step();

  void obtain_column_value(size_t index, int & value) const;
  void obtain_column_value(size_t index, rcutils_time_point_value_t & value) const;
  void obtain_column_value(size_t index, double & value) const;
  void obtain_column_value(size_t index, std::string & value) const;
  void obtain_column_value(size_t index, std::shared_ptr<rcutils_char_array_t> & value) const;

  template<typename T>
  void check_and_report_bind_error(int return_code, T value);
  void check_and_report_bind_error(int return_code);

  sqlite3_stmt * statement_;
  int last_bound_parameter_index_;
  std::vector<std::shared_ptr<rcutils_char_array_t>> written_blobs_cache_;
};

template<typename T1, typename T2, typename ... Params>
inline
std::shared_ptr<SqliteStatementWrapper>
SqliteStatementWrapper::bind(T1 value1, T2 value2, Params ... values)
{
  bind(value1);
  return bind(value2, values ...);
}

template<>
inline
void SqliteStatementWrapper::check_and_report_bind_error(int return_code, std::string value)
{
  if (return_code != SQLITE_OK) {
    throw SqliteException("SQLite error when binding parameter " +
            std::to_string(last_bound_parameter_index_) + " to value '" + value +
            "'. Return code: " + std::to_string(return_code));
  }
}

template<typename T>
inline
void SqliteStatementWrapper::check_and_report_bind_error(int return_code, T value)
{
  check_and_report_bind_error(return_code, std::to_string(value));
}

template<typename ... Columns>
inline
SqliteStatementWrapper::QueryResult<Columns ...> SqliteStatementWrapper::execute_query()
{
  return QueryResult<Columns ...>(shared_from_this());
}

using SqliteStatement = std::shared_ptr<SqliteStatementWrapper>;

}  // namespace rosbag2_storage_plugins

#endif  // ROSBAG2_STORAGE_DEFAULT_PLUGINS__SQLITE__SQLITE_STATEMENT_WRAPPER_HPP_