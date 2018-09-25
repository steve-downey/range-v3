/// \file
// Range v3 library
//
//  Copyright Eric Niebler 2014-present
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//

#ifndef RANGES_V3_VIEW_JOIN_HPP
#define RANGES_V3_VIEW_JOIN_HPP

#include <utility>
#include <type_traits>
#include <meta/meta.hpp>
#include <range/v3/detail/satisfy_boost_range.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/size.hpp>
#include <range/v3/numeric.hpp> // for accumulate
#include <range/v3/begin_end.hpp>
#include <range/v3/empty.hpp>
#include <range/v3/range_traits.hpp>
#include <range/v3/utility/functional.hpp>
#include <range/v3/utility/static_const.hpp>
#include <range/v3/utility/variant.hpp>
#include <range/v3/view_facade.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/view.hpp>
#include <range/v3/view/single.hpp>

namespace ranges
{
    inline namespace v3
    {
        /// \cond
        namespace detail
        {
            // Compute the cardinality of a joined range
            template<typename Outer, typename Inner, typename Joiner>
            using join_cardinality_ =
                std::integral_constant<cardinality,
                    Outer::value == infinite || Inner::value == infinite || (Joiner::value == infinite && Outer::value != 0 && Outer::value != 1) ?
                        infinite :
                        Outer::value == unknown || Inner::value == unknown || (Joiner::value == unknown && Outer::value != 0 && Outer::value != 1) ?
                            unknown :
                            Outer::value == finite || Inner::value == finite || (Joiner::value == finite && Outer::value != 0 && Outer::value != 1) ?
                                finite :
                                static_cast<cardinality>(Outer::value * Inner::value + (Outer::value == 0 ? 0 : (Outer::value - 1) * Joiner::value))>;
            template<typename Range, typename JoinRange = void>
            struct join_cardinality :
                join_cardinality_<range_cardinality<Range>, range_cardinality<range_reference_t<Range>>,
                    meta::if_<std::is_same<void, JoinRange>,
                        std::integral_constant<cardinality, static_cast<cardinality>(0)>,
                        range_cardinality<JoinRange>>>
            {};
        }
        /// \endcond

        /// \addtogroup group-views
        /// @{

        // Join a range of ranges
        template<typename Rng>
        struct join_view<Rng, void>
          : view_facade<join_view<Rng, void>, detail::join_cardinality<Rng>::value>
        {
            CPP_assert(InputRange<Rng>);
            CPP_assert(InputRange<range_reference_t<Rng>>);
            using size_type = common_type_t<range_size_type_t<Rng>, range_size_type_t<range_reference_t<Rng>>>;

            join_view() = default;
            explicit join_view(Rng rng)
              : outer_(view::all(std::move(rng)))
            {}
            CPP_member
            constexpr auto size() const -> CPP_ret(size_type)(
                requires detail::join_cardinality<Rng>::value >= 0)
            {
                return detail::join_cardinality<Rng>::value;
            }
            CPP_member
            RANGES_CXX14_CONSTEXPR auto size() -> CPP_ret(size_type)(
                requires detail::join_cardinality<Rng>::value < 0 &&
                    range_cardinality<Rng>::value >= 0 &&
                    (bool) ForwardRange<Rng> &&
                    (bool) SizedRange<range_reference_t<Rng>>)
            {
                return accumulate(view::transform(outer_, ranges::size), size_type{0});
            }
        private:
            friend range_access;
            using Outer = view::all_t<Rng>;
            using Inner = view::all_t<range_reference_t<Outer>>;

            Outer outer_{};
            Inner inner_{};

            class cursor
            {
            private:
                join_view* rng_ = nullptr;
                iterator_t<Outer> outer_it_{};
                iterator_t<Inner> inner_it_{};

                void satisfy()
                {
                    while(inner_it_ == ranges::end(rng_->inner_) &&
                         ++outer_it_ != ranges::end(rng_->outer_))
                    {
                        rng_->inner_ = view::all(*outer_it_);
                        inner_it_ = ranges::begin(rng_->inner_);
                    }
                }
            public:
                using single_pass = std::true_type;
                cursor() = default;
                cursor(join_view &rng)
                  : rng_{&rng}
                  , outer_it_(ranges::begin(rng.outer_))
                {
                    if(outer_it_ != ranges::end(rng_->outer_))
                    {
                        rng.inner_ = view::all(*outer_it_);
                        inner_it_ = ranges::begin(rng.inner_);
                        satisfy();
                    }
                }
                bool equal(default_sentinel) const
                {
                    return outer_it_ == ranges::end(rng_->outer_);
                }
                void next()
                {
                    RANGES_ASSERT(inner_it_ != ranges::end(rng_->inner_));
                    ++inner_it_;
                    satisfy();
                }
                auto read() const
                RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
                (
                    *inner_it_
                )
                auto move() const
                RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
                (
                    iter_move(inner_it_)
                )
            };
            cursor begin_cursor()
            {
                return cursor{*this};
            }
            // TODO: could support const iteration if range_reference_t<Rng> is a true reference.
        };

        // Join a range of ranges, inserting a range of values between them.
        template<typename Rng, typename ValRng>
        struct join_view
          : view_facade<join_view<Rng, ValRng>, detail::join_cardinality<Rng, ValRng>::value>
        {
            CPP_assert(InputRange<Rng>);
            CPP_assert(InputRange<range_reference_t<Rng>>);
            CPP_assert(ForwardRange<ValRng>);
            CPP_assert(Common<range_value_type_t<range_reference_t<Rng>>, range_value_type_t<ValRng>>);
            CPP_assert(Semiregular<common_type_t<
                range_value_type_t<range_reference_t<Rng>>,
                range_value_type_t<ValRng>>>);
            using size_type = common_type_t<range_size_type_t<Rng>, range_size_type_t<range_value_type_t<Rng>>>;

            join_view() = default;
            join_view(Rng rng, ValRng val)
              : outer_(view::all(std::move(rng)))
              , val_(view::all(std::move(val)))
            {}
            CPP_member
            constexpr auto size() const -> CPP_ret(size_type)(
                requires detail::join_cardinality<Rng, ValRng>::value >= 0)
            {
                return detail::join_cardinality<Rng, ValRng>::value;
            }
            CPP_member
            auto size() const -> CPP_ret(size_type)(
                requires detail::join_cardinality<Rng, ValRng>::value < 0 &&
                    range_cardinality<Rng>::value >= 0 && ForwardRange<Rng> &&
                    SizedRange<range_reference_t<Rng>> && SizedRange<ValRng>)
            {
                return accumulate(view::transform(outer_, ranges::size), size_type{0}) +
                        (range_cardinality<Rng>::value == 0 ?
                            0 :
                            ranges::size(val_) * (range_cardinality<Rng>::value - 1));;
            }
        private:
            friend range_access;
            using Outer = view::all_t<Rng>;
            using Inner = view::all_t<range_reference_t<Outer>>;

            Outer outer_{};
            Inner inner_{};
            view::all_t<ValRng> val_{};

            class cursor
            {
                join_view* rng_ = nullptr;
                iterator_t<Outer> outer_it_{};
                variant<iterator_t<ValRng>, iterator_t<Inner>> cur_{};

                void satisfy()
                {
                    while(true)
                    {
                        if(cur_.index() == 0)
                        {
                            if(ranges::get<0>(cur_) != ranges::end(rng_->val_))
                                break;
                            rng_->inner_ = view::all(*outer_it_);
                            ranges::emplace<1>(cur_, ranges::begin(rng_->inner_));
                        }
                        else
                        {
                            if(ranges::get<1>(cur_) != ranges::end(rng_->inner_))
                                break;
                            if(++outer_it_ == ranges::end(rng_->outer_))
                                break;
                            ranges::emplace<0>(cur_, ranges::begin(rng_->val_));
                        }
                    }
                }
            public:
                using value_type = common_type_t<
                    range_value_type_t<Inner>, range_value_type_t<ValRng>>;
                using reference = common_reference_t<
                    range_reference_t<Inner>, range_reference_t<ValRng>>;
                using rvalue_reference = common_reference_t<
                    range_rvalue_reference_t<Inner>, range_rvalue_reference_t<ValRng>>;
                using single_pass = std::true_type;
                cursor() = default;
                cursor(join_view &rng)
                  : rng_{&rng}
                  , outer_it_(ranges::begin(rng.outer_))
                {
                    if(outer_it_ != ranges::end(rng_->outer_))
                    {
                        rng.inner_ = view::all(*outer_it_);
                        ranges::emplace<1>(cur_, ranges::begin(rng.inner_));
                        satisfy();
                    }
                }
                bool equal(default_sentinel) const
                {
                    return outer_it_ == ranges::end(rng_->outer_);
                }
                void next()
                {
                    // visit(cur_, [](auto& it){ ++it; });
                    if(cur_.index() == 0)
                    {
                        auto& it = ranges::get<0>(cur_);
                        RANGES_ASSERT(it != ranges::end(rng_->val_));
                        ++it;
                    }
                    else
                    {
                        auto& it = ranges::get<1>(cur_);
                        RANGES_ASSERT(it != ranges::end(rng_->inner_));
                        ++it;
                    }
                    satisfy();
                }
                reference read() const
                {
                    // return visit(cur_, [](auto& it) -> reference { return *it; });
                    if(cur_.index() == 0)
                    {
                        return *ranges::get<0>(cur_);
                    }
                    else
                    {
                        return *ranges::get<1>(cur_);
                    }
                }
                rvalue_reference move() const
                {
                    // return visit(cur_, [](auto& it) -> rvalue_reference { return iter_move(it); });
                    if(cur_.index() == 0)
                    {
                        return iter_move(ranges::get<0>(cur_));
                    }
                    else
                    {
                        return iter_move(ranges::get<1>(cur_));
                    }
                }
            };
            cursor begin_cursor()
            {
                return {*this};
            }
            // TODO: could support const iteration if range_reference_t<Rng> is a true reference.
        };

        namespace view
        {
            // Don't forget to update view::for_each whenever this set
            // of concepts changes
            CPP_def
            (
                template(typename Rng)
                concept JoinableRange,
                    InputRange<Rng> && InputRange<range_reference_t<Rng>>
            );

            struct join_fn
            {
                CPP_template(typename Rng)(
                    requires JoinableRange<Rng>)
                join_view<all_t<Rng>> operator()(Rng &&rng) const
                {
                    return join_view<all_t<Rng>>{all(static_cast<Rng &&>(rng))};
                }
                CPP_template(typename Rng, typename Val = range_value_type_t<range_reference_t<Rng>>)(
                    requires JoinableRange<Rng>)
                join_view<all_t<Rng>, single_view<Val>> operator()(Rng &&rng, meta::id_t<Val> v) const
                {
                    CPP_assert_msg(Semiregular<Val>,
                        "To join a range of ranges with a value, the value type must be a model of "
                        "the Semiregular concept; that is, it must have a default constructor, "
                        "copy and move constructors, and a destructor.");
                    return {all(static_cast<Rng &&>(rng)), single(std::move(v))};
                }
                CPP_template(typename Rng, typename ValRng)(
                    // For some reason, this gives gcc problems:
                    //requires JoinableRange<Rng> && ForwardRange<ValRng>)
                    requires JoinableRange<Rng> && Range<ValRng> && ForwardIterator<iterator_t<ValRng>>)
                join_view<all_t<Rng>, all_t<ValRng>> operator()(Rng &&rng, ValRng &&val) const
                {
                    CPP_assert_msg(Common<range_value_type_t<ValRng>,
                        range_value_type_t<range_reference_t<Rng>>>,
                        "To join a range of ranges with another range, all the ranges must have "
                        "a common value type.");
                    CPP_assert_msg(Semiregular<common_type_t<
                        range_value_type_t<ValRng>, range_value_type_t<range_reference_t<Rng>>>>,
                        "To join a range of ranges with another range, all the ranges must have "
                        "a common value type, and that value type must model the Semiregular "
                        "concept; that is, it must have a default constructor, copy and move "
                        "constructors, and a destructor.");
                    return {all(static_cast<Rng &&>(rng)), all(static_cast<ValRng &&>(val))};
                }
            private:
               friend view_access;
               CPP_template(typename T)(
                   requires not JoinableRange<T>)
               static auto bind(join_fn join, T &&t)
               RANGES_DECLTYPE_AUTO_RETURN
               (
                   make_pipeable(std::bind(join, std::placeholders::_1, bind_forward<T>(t)))
               )
            };

            /// \relates join_fn
            /// \ingroup group-views
            RANGES_INLINE_VARIABLE(view<join_fn>, join)
        }
        /// @}
    }
}

RANGES_SATISFY_BOOST_RANGE(::ranges::v3::join_view)

#endif
