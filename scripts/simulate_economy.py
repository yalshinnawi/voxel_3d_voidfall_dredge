#!/usr/bin/env python3
"""
Voidfall Dredge - Progression & Economy Curve Simulator
Simulates player level progression, coin expedition yields, and verifies upgrade cost models.
"""

import math

COIN_COSTS = [100, 160, 256, 410, 656]
LEVEL_THRESHOLDS = [0, 300, 700, 1300, 2200, 3700]

def get_coin_cost(current_tier: int) -> int:
    if current_tier >= len(COIN_COSTS):
        return 0
    return COIN_COSTS[current_tier]

def get_voidite_cost(current_tier: int) -> int:
    if current_tier >= len(COIN_COSTS):
        return 0
    return 4 * (current_tier + 1)

def get_level_for_exp(exp: int) -> int:
    lvl = 1
    for i, threshold in enumerate(LEVEL_THRESHOLDS):
        if exp >= threshold:
            lvl = i + 1
    return lvl

def verify_progression_invariants():
    print("=" * 65)
    print(" Voidfall Dredge - Dual Economy & Level Progression Invariant Check")
    print("=" * 65)
    
    cumulative_coins = 0
    cumulative_voidite = 0
    
    print("[1] Upgrade Tier Matrix (Coins + Minerals + Level Gating):")
    for tier in range(len(COIN_COSTS)):
        cost_coins = get_coin_cost(tier)
        cost_voidite = get_voidite_cost(tier)
        cumulative_coins += cost_coins
        cumulative_voidite += cost_voidite
        req_lvl = tier + 1
        print(f" Tier {tier+1} (Req Lv {req_lvl}): {cost_coins:3d} Coins | {cost_voidite:2d} Voidite  (Cumulative: {cumulative_coins:4d} Coins, {cumulative_voidite:2d} Voidite)")
        
    print("-" * 65)
    refund_coins = round(cumulative_coins * 0.85)
    loss_coins = cumulative_coins - refund_coins
    print(f" Total to max 1 branch (5 Tiers): {cumulative_coins} Coins, {cumulative_voidite} Voidite")
    print(f" Respec refund (85%): {refund_coins} Coins (Penalty loss: {loss_coins} Coins)")
    print("-" * 65)
    
    total_tree_coins = cumulative_coins * 6
    total_tree_voidite = cumulative_voidite * 6
    print(f" Total to max ALL 6 branches: {total_tree_coins} Coins, {total_tree_voidite} Voidite")
    
    # Average expedition yield
    avg_coins = 120
    runs_to_max_all = math.ceil(total_tree_coins / avg_coins)
    print(f" Estimated expeditions to complete full tree: ~{runs_to_max_all} successful runs")
    
    print("-" * 65)
    print("[2] Player Rank Level Curve (Cumulative EXP):")
    for lvl, exp in enumerate(LEVEL_THRESHOLDS, 1):
        sector_unlock = "Sector 1 (Perimeter)" if lvl == 1 else "Sector 2 (Fault)" if lvl == 2 else "Sector 3 (Cradle)" if lvl == 4 else ""
        class_unlock = "Demolitionist" if lvl == 1 else "Vanguard" if lvl == 2 else "Scout" if lvl == 3 else ""
        unlocks = ", ".join(filter(None, [sector_unlock, class_unlock]))
        print(f" Delver Rank Lv {lvl}: {exp:4d} EXP Required {'-> Unlocks: ' + unlocks if unlocks else ''}")
    print("=" * 65)
    return True

if __name__ == "__main__":
    verify_progression_invariants()
