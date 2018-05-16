#ifndef CONFLUO_PARSER_EXPRESSION_COMPILER_H_
#define CONFLUO_PARSER_EXPRESSION_COMPILER_H_

#include "schema/schema.h"
#include "schema/schema_snapshot.h"
#include "expression_parser.h"

namespace confluo {
namespace parser {

namespace spirit = boost::spirit;

/**
 * Compiled predicate class. Contains operations for extracting data
 * from the expression.
 */
struct compiled_predicate {
   /**
     * Constructs a predicate from the specified fields
     *
     * @param attr The field of the predicate
     * @param op The operator of the predicate
     * @param value The value of the predicate
     * @param s The schema which contains the attribute
     */
  compiled_predicate(const std::string& attr, int op, const std::string& value,
                     const schema_t& s)
      : field_name_(s[attr].name()),
        field_idx_(s[attr].idx()),
        op_(static_cast<reational_op_id>(op)),
        val_(mutable_value::parse(value, s[attr].type())) {
  }

  /**
   * Gets the field name
   *
   * @return A string containing the field name
   */
  inline std::string const& field_name() const {
    return field_name_;
  }

  /**
   * Gets the index of the field
   *
   * @return The index of the field
   */
  inline uint32_t field_idx() const {
    return field_idx_;
  }

  /**
   * Gets the relational operator
   *
   * @return The identifier for the relational operator
   */
  inline reational_op_id op() const {
    return op_;
  }

  /**
   * Gets the immutable value for the predicate
   *
   * @return The immutable value in the compiled predicate
   */
  inline immutable_value const& value() const {
    return val_;
  }

  /**
   * Performs the relational operation on the value and the specified
   * record
   *
   * @param r The record used in the relational operator
   *
   * @return True if the relational operation is true, false otherwise
   */
  inline bool test(const record_t& r) const {
    return immutable_value::relop(op_, r[field_idx_].value(), val_);
  }

  /**
   * Performs the relational operation on the value and the specified
   * schema snapshot and data
   *
   * @param snap The schema snapshot to get the data from
   * @param data The data used for the comparison
   *
   * @return True if the relational operation is true, false otherwise
   */
  inline bool test(const schema_snapshot& snap, void* data) const {
    return immutable_value::relop(op_, snap.get(data, field_idx_), val_);
  }

  /**
   * Gets a string representation of the compiled predicate
   *
   * @return A string containing the contents of the predicate
   */
  inline std::string to_string() const {
    return field_name_ + relop_utils::op_to_str(op_) + val_.to_string();
  }

  /**
   * The less than operator that compares this compiled predicate to
   * another compiled predicate
   *
   * @param other The other compiled predicate used for comparison
   *
   * @return True if this compiled predicate is less than the other
   * compiled predicate, false otherwise
   */
  inline bool operator<(const compiled_predicate& other) const {
    return to_string() < other.to_string();
  }

 private:
  std::string field_name_;
  uint32_t field_idx_;
  reational_op_id op_;
  mutable_value val_;
};

/**
 * A set of compiled predicates. Manages a list of predicate expressions.
 */
struct compiled_minterm : public std::set<compiled_predicate> {
  /**
   * Adds a compiled predicate to this compiled minterm
   *
   * @param p The predicate to add
   */
  inline void add(const compiled_predicate& p) {
    insert(p);
  }

  /**
   * Adds a r value compiled predicate to the compiled minterm
   *
   * @param p The r value predicate to add to the minterm
   */
  inline void add(compiled_predicate&& p) {
    insert(std::move(p));
  }

  /**
   * Tests every predicate in the set against the record
   *
   * @param r The record to test the predicate on
   *
   * @return True if all of the predicates tests are true, false otherwise
   */
  inline bool test(const record_t& r) const {
    for (auto& p : *this)
      if (!p.test(r))
        return false;
    return true;
  }

  /**
   * Tests every predicate against the data in the schema snapshot
   *
   * @param snap The schema snapshot to look at
   * @param data The data the predicates are tested against
   *
   * @return True if all of the predicates tests are true, false otherwise
   */
  inline bool test(const schema_snapshot& snap, void* data) const {
    for (auto& p : *this)
      if (!p.test(snap, data))
        return false;
    return true;
  }

  /**
   * Gets a string representation of the compiled minterm
   *
   * @return A string with the contents of the compiled minterm
   */
  std::string to_string() const {
    std::string s = "";
    size_t i = 0;
    for (auto& p : *this) {
      s += p.to_string();
      if (++i < size())
        s += " and ";
    }
    return s;
  }

  /**
   * Performs a less than comparison with another compiled minterm
   *
   * @param other The other compiled minterm to perform a comparison with
   *
   * @return True if this compiled minterm is less than the other compiled
   * minterm, false otherwise
   */
  inline bool operator<(const compiled_minterm& other) const {
    return to_string() < other.to_string();
  }
};

/**
 * A set of compiled minterms. Manages a grouping of minterms
 */
struct compiled_expression : public std::set<compiled_minterm> {
    /**
     * Tests every compiled minterm in the set against a record
     *
     * @param r The record to test against
     *
     * @return True if every compiled minterm test is true, false otherwise
     */
  inline bool test(const record_t& r) const {
    if (empty())
      return true;

    for (auto& p : *this)
      if (p.test(r))
        return true;

    return false;
  }

  /**
   * Tests every compiled minterm against the data from the schema snapshot
   *
   * @param snap The snapshot of the schema
   * @param data The data that is tested
   *
   * @return True if the all of the compiled minterm tests are true, false
   * otherwise
   */
  inline bool test(const schema_snapshot& snap, void* data) const {
    if (empty())
      return true;

    for (auto& p : *this)
      if (p.test(snap, data))
        return true;

    return false;
  }

  /**
   * Gets a string representation of the compiled expression
   *
   * @return The contents of the compiled expression in string form
   */
  std::string to_string() const {
    std::string ret = "";
    size_t s = size();
    size_t i = 0;
    for (auto& p : *this) {
      ret += p.to_string();
      if (++i < s - 1)
        ret += " or ";
    }
    return ret;
  }
};

/**
 * Conjunction operation
 */
class utree_expand_conjunction {
 public:
  /** The result type */
  typedef compiled_expression result_type;

  /**
   * Expands the conjunction
   *
   * @param m The compiled minterm
   * @param schema The schema for the monolog
   */
  utree_expand_conjunction(const compiled_minterm& m, const schema_t& schema)
      : m_(m),
        schema_(schema) {
  }

  /**
   * Operation for all types exception functions
   * @tparam T The type
   * @throw parse_exception Unrecognized type
   * @return The result
   */
  template<typename T>
  result_type operator()(T) const {
    throw parse_exception(std::string("Unrecognized type ") + typeid(T).name());
  }

  /**
   * Operation for function type
   * @throw parse_exception Functions are not supported
   * @return The result
   */
  result_type operator()(spirit::function_base const&) const {
    throw parse_exception("Functions not supported");
  }

  /**
   * Operation for an iterator range
   *
   * @tparam Iterator type
   * @param range The iterator range
   *
   * @return The result
   */
  template<typename Iterator>
  result_type operator()(boost::iterator_range<Iterator> const& range) const {
    typedef typename boost::iterator_range<Iterator>::const_iterator iterator;
    result_type e;
    iterator i = range.begin();
    int op = spirit::utree::visit(*i, utree_to_op());
    switch (op) {
      case reational_op_id::EQ:
      case reational_op_id::NEQ:
      case reational_op_id::LT:
      case reational_op_id::GT:
      case reational_op_id::LE:
      case reational_op_id::GE: {
        compiled_minterm right = m_;
        std::string attr = spirit::utree::visit(*(++i), utree_to_string());
        std::string value = spirit::utree::visit(*(++i), utree_to_string());
        right.add(compiled_predicate(attr, op, value, schema_));
        e.insert(right);
        break;
      }
      case and_or::OR: {
        compiled_expression left = spirit::utree::visit(*(++i), *this);
        compiled_expression right = spirit::utree::visit(*(++i), *this);
        std::set_union(left.begin(), left.end(), right.begin(), right.end(),
                       std::inserter(e, e.end()));
        break;
      }
      case and_or::AND: {
        compiled_expression lor = spirit::utree::visit(*(++i), *this);
        auto r = *(++i);
        for (auto& lor_m : lor) {
          result_type tmp = spirit::utree::visit(
              r, utree_expand_conjunction(lor_m, schema_));
          std::set_union(e.begin(), e.end(), tmp.begin(), tmp.end(),
                         std::inserter(e, e.end()));
        }
        break;
      }
      default: {
        throw parse_exception("Unexpected op:" + std::to_string(op));
      }
    }
    return e;
  }

 private:
  const compiled_minterm& m_;
  const schema_t& schema_;
};

/**
 * Tree Compiled expression class. 
 * Manages operations performed on expressions
 */
class utree_compile_expression {
 public:
  /** The evaluated compiled expression */
  typedef compiled_expression result_type;

  /**
   * Constructs a compiled expression from the given schema
   *
   * @param schema The schema used to initialize the compiled expression
   */
  utree_compile_expression(const schema_t& schema)
      : schema_(schema) {
  }

  /**
   * () operator that evaluates the compiled expression
   * @tparam T The type operator is called on
   * @throw parse_exception
   * @return The result of the operator
   */
  template<typename T>
  result_type operator()(T) const {
    throw parse_exception(std::string("Unrecognized type ") + typeid(T).name());
  }

  /**
   * () operator that evaluates the compiled expression
   * @throw parse_exception
   * @return The result of the operator
   */
  result_type operator()(spirit::function_base const&) const {
    throw parse_exception("Functions not supported");
  }

  /**
   * () operator that evaluates the compiled expression
   *
   * @tparam Iterator
   * @param range The range of tokens to evaluate
   *
   * @return The result of evaluating the expression
   */
  template<typename Iterator>
  result_type operator()(boost::iterator_range<Iterator> const& range) const {
    typedef typename boost::iterator_range<Iterator>::const_iterator iterator;
    result_type e;
    iterator i = range.begin();
    int op = spirit::utree::visit(*i, utree_to_op());
    switch (op) {
      case reational_op_id::EQ:
      case reational_op_id::NEQ:
      case reational_op_id::LT:
      case reational_op_id::GT:
      case reational_op_id::LE:
      case reational_op_id::GE: {
        compiled_minterm m;
        std::string attr = spirit::utree::visit(*(++i), utree_to_string());
        std::string value = spirit::utree::visit(*(++i), utree_to_string());
        m.add(compiled_predicate(attr, op, value, schema_));
        e.insert(m);
        break;
      }
      case and_or::OR: {
        result_type left = spirit::utree::visit(*(++i), *this);
        result_type right = spirit::utree::visit(*(++i), *this);
        std::set_union(left.begin(), left.end(), right.begin(), right.end(),
                       std::inserter(e, e.end()));
        break;
      }
      case and_or::AND: {
        result_type left = spirit::utree::visit(*(++i), *this);
        auto r = *(++i);
        for (auto& m : left) {
          result_type tmp = spirit::utree::visit(
              r, utree_expand_conjunction(m, schema_));
          std::set_union(e.begin(), e.end(), tmp.begin(), tmp.end(),
                         std::inserter(e, e.end()));
        }
        break;
      }
      default: {
        throw parse_exception("Unexpected op:" + std::to_string(op));
      }
    }
    return e;
  }

 private:
  const schema_t& schema_;
};

/**
 * Recursively gets the compiled expression from the tree and schema
 *
 * @param e The utree containing the contents of the expression
 * @param schema The schema to create the compiled expression from
 *
 * @return Compiled expression containing the result of evaluation
 */
static compiled_expression compile_expression(const spirit::utree& e,
                                              const schema_t& schema) {
  return spirit::utree::visit(e, utree_compile_expression(schema));
}

}
}

#endif /* CONFLUO_PARSER_EXPRESSION_COMPILER_H_ */
