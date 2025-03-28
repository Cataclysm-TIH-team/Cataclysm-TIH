#include "character.h"
#include "creature_tracker.h"
#include "flag.h"
#include "map.h"
#include "map_iterator.h"
#include "messages.h"
#include "monster.h"
#include "mtype.h"
#include "options.h"
#include "output.h"

static const efftype_id effect_beartrap( "beartrap" );
static const efftype_id effect_crushed( "crushed" );
static const efftype_id effect_downed( "downed" );
static const efftype_id effect_grabbed( "grabbed" );
static const efftype_id effect_grabbing( "grabbing" );
static const efftype_id effect_heavysnare( "heavysnare" );
static const efftype_id effect_in_pit( "in_pit" );
static const efftype_id effect_lightsnare( "lightsnare" );
static const efftype_id effect_stunned( "stunned" );
static const efftype_id effect_webbed( "webbed" );

static const itype_id itype_rope_6( "rope_6" );
static const itype_id itype_snare_trigger( "snare_trigger" );
static const itype_id itype_string_36( "string_36" );

static const json_character_flag json_flag_DOWNED_RECOVERY( "DOWNED_RECOVERY" );

static const limb_score_id limb_score_balance( "balance" );
static const limb_score_id limb_score_grip( "grip" );
static const limb_score_id limb_score_manip( "manip" );

bool Character::can_escape_trap( int difficulty, bool manip = false ) const
{
    int chance = get_arm_str();
    chance *= manip ? get_limb_score( limb_score_manip ) : get_limb_score( limb_score_grip );
    return x_in_y( chance, difficulty );
}

void Character::try_remove_downed()
{

    /** @EFFECT_DEX increases chance to stand up when knocked down */
    /** @EFFECT_ARM_STR increases chance to stand up when knocked down, slightly */
    // Downed reduces balance score to 10% unless resisted, multiply to compensate
    int chance = ( get_dex() + get_arm_str() / 2.0 ) * get_limb_score( limb_score_balance ) * 10.0;
    // Always 2,5% chance to stand up
    chance += has_flag( json_flag_DOWNED_RECOVERY ) ? 20 : 1;
    if( !x_in_y( chance, 40 ) ) {
        add_msg_if_player( _( "You struggle to stand." ) );
    } else {
        add_msg_player_or_npc( m_good,
                               has_flag( json_flag_DOWNED_RECOVERY ) ? _( "You deftly roll to your feet." ) : _( "You stand up." ),
                               get_option<bool>( "LOG_MONSTER_ATTACK_MONSTER" ) ?
                               has_flag( json_flag_DOWNED_RECOVERY ) ? _( "<npcname> deftly rolls to their feet." ) :
                               _( "<npcname> stands up." ) : "" );
        remove_effect( effect_downed );
    }
}

void Character::try_remove_bear_trap()
{
    /* Real bear traps can't be removed without the proper tools or immense strength; eventually this should
       allow normal players two options: removal of the limb or removal of the trap from the ground
       (at which point the player could later remove it from the leg with the right tools).
       As such we are currently making it a bit easier for players and NPC's to get out of bear traps.
    */
    // If is riding, then despite the character having the effect, it is the mounted creature that escapes.
    if( is_avatar() && is_mounted() ) {
        auto *mon = mounted_creature.get();
        if( mon->type->melee_dice * mon->type->melee_sides >= 18 ) {
            if( x_in_y( mon->type->melee_dice * mon->type->melee_sides, 200 ) ) {
                mon->remove_effect( effect_beartrap );
                remove_effect( effect_beartrap );
                add_msg( _( "The %s escapes the bear trap!" ), mon->get_name() );
            } else {
                add_msg_if_player( m_bad,
                                   _( "Your %s tries to free itself from the bear trap, but can't get loose!" ), mon->get_name() );
            }
        }
    } else {
        if( can_escape_trap( 100 ) ) {
            remove_effect( effect_beartrap );
            add_msg_player_or_npc( m_good, _( "You free yourself from the bear trap!" ),
                                   get_option<bool>( "LOG_MONSTER_ATTACK_MONSTER" ) ?
                                   _( "<npcname> frees themselves from the bear trap!" ) : "" );
        } else {
            add_msg_if_player( m_bad,
                               _( "You try to free yourself from the bear trap, but can't get loose!" ) );
        }
    }
}

void Character::try_remove_lightsnare()
{
    map &here = get_map();
    if( is_mounted() ) {
        auto *mon = mounted_creature.get();
        if( x_in_y( mon->type->melee_dice * mon->type->melee_sides, 12 ) ) {
            mon->remove_effect( effect_lightsnare );
            remove_effect( effect_lightsnare );
            here.spawn_item( pos(), itype_string_36 );
            here.spawn_item( pos(), itype_snare_trigger );
            add_msg( _( "The %s escapes the light snare!" ), mon->get_name() );
        }
    } else {
        if( can_escape_trap( 12 ) ) {
            remove_effect( effect_lightsnare );
            add_msg_player_or_npc( m_good, _( "You free yourself from the light snare!" ),
                                   get_option<bool>( "LOG_MONSTER_ATTACK_MONSTER" ) ?
                                   _( "<npcname> frees themselves from the light snare!" ) : "" );
            item string( "string_36", calendar::turn );
            item snare( "snare_trigger", calendar::turn );
            here.add_item_or_charges( pos(), string );
            here.add_item_or_charges( pos(), snare );
        } else {
            add_msg_if_player( m_bad,
                               _( "You try to free yourself from the light snare, but can't get loose!" ) );
        }
    }
}

void Character::try_remove_heavysnare()
{
    map &here = get_map();
    if( is_mounted() ) {
        auto *mon = mounted_creature.get();
        if( mon->type->melee_dice * mon->type->melee_sides >= 7 ) {
            if( x_in_y( mon->type->melee_dice * mon->type->melee_sides, 32 ) ) {
                mon->remove_effect( effect_heavysnare );
                remove_effect( effect_heavysnare );
                here.spawn_item( pos(), itype_rope_6 );
                here.spawn_item( pos(), itype_snare_trigger );
                add_msg( _( "The %s escapes the heavy snare!" ), mon->get_name() );
            }
        }
    } else {
        if( can_escape_trap( 32 - dex_cur, true ) ) {
            remove_effect( effect_heavysnare );
            add_msg_player_or_npc( m_good, _( "You free yourself from the heavy snare!" ),
                                   get_option<bool>( "LOG_MONSTER_ATTACK_MONSTER" ) ?
                                   _( "<npcname> frees themselves from the heavy snare!" ) : "" );
            item rope( "rope_6", calendar::turn );
            item snare( "snare_trigger", calendar::turn );
            here.add_item_or_charges( pos(), rope );
            here.add_item_or_charges( pos(), snare );
        } else {
            add_msg_if_player( m_bad,
                               _( "You try to free yourself from the heavy snare, but can't get loose!" ) );
        }
    }
}

void Character::try_remove_crushed()
{
    if( can_escape_trap( 100 ) ) {
        remove_effect( effect_crushed );
        add_msg_player_or_npc( m_good, _( "You free yourself from the rubble!" ),
                               get_option<bool>( "LOG_MONSTER_ATTACK_MONSTER" ) ?
                               _( "<npcname> frees themselves from the rubble!" ) : "" );
    } else {
        add_msg_if_player( m_bad, _( "You try to free yourself from the rubble, but can't get loose!" ) );
    }
}

bool Character::try_remove_grab()
{
    int zed_number = 0;
    if( is_mounted() ) {
        auto *mon = mounted_creature.get();
        if( mon->has_effect( effect_grabbed ) ) {
            if( ( dice( mon->type->melee_dice + mon->type->melee_sides,
                        3 ) < get_effect_int( effect_grabbed ) ) ||
                !one_in( 4 ) ) {
                add_msg( m_bad, _( "Your %s tries to break free, but fails!" ), mon->get_name() );
                return false;
            } else {
                add_msg( m_good, _( "Your %s breaks free from the grab!" ), mon->get_name() );
                remove_effect( effect_grabbed );
                mon->remove_effect( effect_grabbed );
            }
        } else {
            if( one_in( 4 ) ) {
                add_msg( m_bad, _( "You are pulled from your %s!" ), mon->get_name() );
                remove_effect( effect_grabbed );
                forced_dismount();
            }
        }
    } else {
        map &here = get_map();
        creature_tracker &creatures = get_creature_tracker();
        for( auto&& dest : here.points_in_radius( pos(), 1, 0 ) ) { // *NOPAD*
            const monster *const mon = creatures.creature_at<monster>( dest );
            if( mon && mon->has_effect( effect_grabbing ) ) {
                zed_number += mon->get_grab_strength();
            }
        }

        /** @EFFECT_STR increases chance to escape grab */
        /** @EFFECT_DEX increases chance to escape grab */
        int defender_check = rng( 0, std::max( get_arm_str(), get_dex() ) );
        int attacker_check = rng( get_effect_int( effect_grabbed, body_part_torso ), 8 );

        // Defender check is modified by the relevant scores
        defender_check *= get_limb_score( limb_score_balance );
        // Monster check is modified by number
        attacker_check *= zed_number;

        if( has_grab_break_tec() ) {
            defender_check = defender_check + 2;
        }

        if( get_effect_int( effect_stunned ) ) {
            defender_check = defender_check - 2;
        }

        if( get_effect_int( effect_downed ) ) {
            defender_check /= 2;
        }

        if( zed_number == 0 ) {
            add_msg_player_or_npc( m_good, _( "You find yourself no longer grabbed." ),
                                   get_option<bool>( "LOG_MONSTER_ATTACK_MONSTER" ) ?
                                   _( "<npcname> finds themselves no longer grabbed." ) : "" );
            remove_effect( effect_grabbed );

            /** @EFFECT_STR increases chance to escape grab */
        } else if( defender_check < attacker_check ) {
            add_msg_player_or_npc( m_bad, _( "You try to break out of the grab, but fail!" ),
                                   get_option<bool>( "LOG_MONSTER_ATTACK_MONSTER" ) ?
                                   _( "<npcname> tries to break out of the grab, but fails!" ) : "" );
            return false;
        } else {
            std::vector<item_pocket *> pd = worn.grab_drop_pockets();
            // if we have items that can be pulled off
            if( !pd.empty() ) {
                // choose an item to be ripped off
                int index = rng( 0, pd.size() - 1 );
                int chance = rng( 0, get_effect_int( effect_grabbed, body_part_torso ) );
                int sturdiness = rng( 0, pd[index]->get_pocket_data()->ripoff );
                // the item is ripped off your character
                if( sturdiness < chance ) {
                    pd[index]->spill_contents( adjacent_tile() );
                    add_msg_player_or_npc( m_bad,
                                           _( "As you escape the grab something comes loose and falls to the ground!" ),
                                           get_option<bool>( "LOG_MONSTER_ATTACK_MONSTER" ) ?
                                           _( "<npcname> escapes the grab something comes loose and falls to the ground!" ) : "" );
                    if( is_avatar() ) {
                        popup( _( "As you escape the grab something comes loose and falls to the ground!" ) );
                    }
                }
            }

            add_msg_player_or_npc( m_good, _( "You break out of the grab!" ),
                                   get_option<bool>( "LOG_MONSTER_ATTACK_MONSTER" ) ?
                                   _( "<npcname> breaks out of the grab!" ) : "" );
            remove_effect( effect_grabbed );

            for( auto&& dest : here.points_in_radius( pos(), 1, 0 ) ) { // *NOPAD*
                monster *mon = creatures.creature_at<monster>( dest );
                if( mon && mon->has_effect( effect_grabbing ) ) {
                    mon->remove_effect( effect_grabbing );
                }
            }
        }
    }
    return true;
}

void Character::try_remove_webs()
{
    if( is_mounted() ) {
        auto *mon = mounted_creature.get();
        if( x_in_y( mon->type->melee_dice * mon->type->melee_sides,
                    6 * get_effect_int( effect_webbed ) ) ) {
            add_msg( _( "The %s breaks free of the webs!" ), mon->get_name() );
            mon->remove_effect( effect_webbed );
            remove_effect( effect_webbed );
        }
    } else if( can_escape_trap( 6 * get_effect_int( effect_webbed ) ) ) {
        add_msg_player_or_npc( m_good, _( "You free yourself from the webs!" ),
                               get_option<bool>( "LOG_MONSTER_ATTACK_MONSTER" ) ?
                               _( "<npcname> frees themselves from the webs!" ) : "" );
        remove_effect( effect_webbed );
    } else {
        add_msg_if_player( _( "You try to free yourself from the webs, but can't get loose!" ) );
    }
}

void Character::try_remove_impeding_effect()
{
    for( const effect &eff : get_effects_with_flag( flag_EFFECT_IMPEDING ) ) {
        const efftype_id &eff_id = eff.get_id();
        if( is_mounted() ) {
            auto *mon = mounted_creature.get();
            if( x_in_y( mon->type->melee_dice * mon->type->melee_sides,
                        6 * get_effect_int( eff_id ) ) ) {
                add_msg( _( "The %s breaks free!" ), mon->get_name() );
                mon->remove_effect( eff_id );
                remove_effect( eff_id );
            }
            /** @EFFECT_STR increases chance to escape webs */
        } else if( can_escape_trap( 6 * get_effect_int( eff_id ) ) ) {
            add_msg_player_or_npc( m_good, _( "You free yourself!" ),
                                   get_option<bool>( "LOG_MONSTER_ATTACK_MONSTER" ) ?
                                   _( "<npcname> frees themselves!" ) : "" );
            remove_effect( eff_id );
        } else {
            add_msg_if_player( _( "You try to free yourself, but can't!" ) );
        }
    }
}

bool Character::move_effects( bool attacking )
{
    if( has_effect( effect_downed ) ) {
        try_remove_downed();
        return false;
    }
    if( has_effect( effect_webbed ) ) {
        try_remove_webs();
        return false;
    }
    if( has_effect( effect_lightsnare ) ) {
        try_remove_lightsnare();
        return false;

    }
    if( has_effect( effect_heavysnare ) ) {
        try_remove_heavysnare();
        return false;
    }
    if( has_effect( effect_beartrap ) ) {
        try_remove_bear_trap();
        return false;
    }
    if( has_effect( effect_crushed ) ) {
        try_remove_crushed();
        return false;
    }
    if( has_effect_with_flag( flag_EFFECT_IMPEDING ) ) {
        try_remove_impeding_effect();
        return false;
    }

    // Below this point are things that allow for movement if they succeed

    // Currently we only have one thing that forces movement if you succeed, should we get more
    // than this will need to be reworked to only have success effects if /all/ checks succeed
    if( has_effect( effect_in_pit ) ) {
        /** @EFFECT_DEX increases chance to escape pit, slightly */
        if( !can_escape_trap( 40 - dex_cur / 2 ) ) {
            add_msg_if_player( m_bad, _( "You try to escape the pit, but slip back in." ) );
            return false;
        } else {
            add_msg_player_or_npc( m_good, _( "You escape the pit!" ),
                                   get_option<bool>( "LOG_MONSTER_ATTACK_MONSTER" ) ?
                                   _( "<npcname> escapes the pit!" ) : "" );
            remove_effect( effect_in_pit );
        }
    }
    return !has_effect( effect_grabbed ) || attacking || try_remove_grab();
}

void Character::wait_effects( bool attacking )
{
    if( has_effect( effect_downed ) ) {
        try_remove_downed();
        return;
    }
    if( has_effect( effect_beartrap ) ) {
        try_remove_bear_trap();
        return;
    }
    if( has_effect( effect_lightsnare ) ) {
        try_remove_lightsnare();
        return;
    }
    if( has_effect( effect_heavysnare ) ) {
        try_remove_heavysnare();
        return;
    }
    if( has_effect( effect_webbed ) ) {
        try_remove_webs();
        return;
    }
    if( has_effect_with_flag( flag_EFFECT_IMPEDING ) ) {
        try_remove_impeding_effect();
        return;
    }
    if( has_effect( effect_grabbed ) && !attacking && !try_remove_grab() ) {
        return;
    }
}

