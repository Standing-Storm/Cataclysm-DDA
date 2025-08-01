#pragma once
#ifndef CATA_SRC_ITEM_LOCATION_H
#define CATA_SRC_ITEM_LOCATION_H

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "coords_fwd.h"
#include "units_fwd.h"

class Character;
class JsonObject;
class JsonOut;
class const_talker;
class item;
class item_pocket;
class map;
class map_cursor;
class talker;
class vehicle_cursor;
template<typename T> class ret_val;

/**
 * A lightweight handle to an item independent of it's location
 * Unlike a raw pointer can be (de-)serialized to/from JSON
 * Provides a generic interface of querying, obtaining and removing an item
 * Is invalidated by many operations (including copying of the item)
 */
class item_location
{
    public:
        enum class type : int {
            invalid = 0,
            character = 1,
            map = 2,
            vehicle = 3,
            container = 4
        };

        item_location();

        static const item_location nowhere;

        item_location( Character &ch, item *which );
        item_location( const map_cursor &mc, item *which );
        item_location( const vehicle_cursor &vc, item *which );
        item_location( const item_location &container, item *which );

        void serialize( JsonOut &js ) const;
        void deserialize( const JsonObject &obj );

        bool operator==( const item_location &rhs ) const;
        bool operator!=( const item_location &rhs ) const;

        explicit operator bool() const;

        item &operator*();
        const item &operator*() const;

        item *operator->();
        const item *operator->() const;

        /** Returns the type of location where the item is found */
        type where() const;

        /** Returns the type of location where the topmost container of the item is found.
         *  Therefore can not return item_location::type::container */
        type where_recursive() const;

        /** Returns the position where the item is found */
        tripoint_bub_ms pos_bub( const map &here ) const;
        tripoint_abs_ms pos_abs() const;

        /** Describes the item location
         *  @param ch if set description is relative to character location */
        std::string describe( const Character *ch = nullptr ) const;

        /** Move an item from the location to the character inventory
         *  If the player fails to obtain the item (likely due to a menu) returns item_location{}
         *  @param ch Character who's inventory gets the item
         *  @param qty if specified limits maximum obtained charges
         *  @warning caller should restack inventory if item is to remain in it
         *  @warning all further operations using this class are invalid
         *  @warning it is unsafe to call this within unsequenced operations (see #15542)
         *  @return item_location for the item */
        item_location obtain( Character &ch, int qty = -1 );

        /** Calculate (but do not deduct) number of moves required to obtain an item
         *  @see item_location::obtain */
        int obtain_cost( const Character &ch, int qty = -1 ) const;

        /** Removes the selected item from the game
         *  @warning all further operations using this class are invalid */
        void remove_item();

        /** Handles updates to the item location, mostly for caching. */
        void on_contents_changed();

        void make_active();

        /** Gets the selected item or nullptr */
        item *get_item();
        const item *get_item() const;

        void set_should_stack( bool should_stack ) const;

        /** returns the parent item, or an invalid location if it has no parent */
        item_location parent_item() const;
        item_pocket *parent_pocket() const;

        /** returns the character whose inventory contains this item, nullptr if none **/
        Character *carrier() const;

        /** returns the character whose inventory contains this item, nullptr if none **/
        const vehicle_cursor *veh_cursor() const;

        /** returns true if the item is in the inventory of the given character **/
        bool held_by( Character const &who ) const;

        /**
         * true if this item location can and does have a parent
         *
         * exists because calling parent_item() naively causes debug messages
         **/
        bool has_parent() const;

        /**
        * Returns whether the item location is inside an e-device)
        */
        bool is_efile() const;

        /**
        * Returns available volume capacity where this item is located.
        */
        units::volume volume_capacity() const;

        /**
        * Returns available weight capacity where this item is located.
        */
        units::mass weight_capacity() const;

        /**
        * Returns true if volume and weight capacity of all parent pockets >= 0
        */
        bool check_parent_capacity_recursive() const;

        /**
        * true if the item is inside a not open watertight container
        **/
        bool protected_from_liquids() const;

        ret_val<void> parents_can_contain_recursive( item *it ) const;
        ret_val<int> max_charges_by_parent_recursive( const item &it ) const;

        /**
         * Returns whether another item is eventually contained by this item
         */
        bool eventually_contains( item_location loc ) const;

        /**
         * Overflow items into parent pockets recursively
         */
        void overflow( map &here );

        /**
         * returns whether the item can be reloaded with the specified item.
         * @param ammo item to be loaded in
         * @param now whether the currently contained ammo/magazine should be taken into account
         */
        bool can_reload_with( const item_location &ammo, bool now ) const;

        /**
        * returns the item's level of the specified quality.
        * @param quality the name of quality to check the level of
        * @param strict_boiling True if containers must be empty to have BOIL quality
        */
        int get_quality( const std::string &quality, bool strict_boiling ) const;

    private:
        class impl;

        std::shared_ptr<impl> ptr;
};
std::unique_ptr<talker> get_talker_for( item_location &it );
std::unique_ptr<const_talker> get_const_talker_for( const item_location &it );
std::unique_ptr<talker> get_talker_for( item_location *it );

using drop_location = std::pair<item_location, int>;
using drop_locations = std::list<drop_location>;

#endif // CATA_SRC_ITEM_LOCATION_H
