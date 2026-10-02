"""
Voidfall Dredge - Progression & Economy Curve Simulator
Simulates player progression, expedition yields, and verifies cost mathematical models.
"""

import math

EXP_COSTS = [100, 160, 256, 410, 656]

def get_exp_cost(current_tier: int) -> int:
    if current_tier >= len(EXP_COSTS):
        return 0
    return EXP_COSTS[current_tier]

def get_voidite_cost(current_tier: int) -> int:
    if current_tier >= len(EXP_COSTS):
        return 0
    return 4 * (current_tier + 1)

def verify_progression_invariants():
    print("=" * 60)
    print(" Voidfall Dredge - Economy & Progression Invariant Check")
    print("=" * 60)
    
    cumulative_exp = 0
    cumulative_voidite = 0
    
    for tier in range(len(EXP_COSTS)):
        cost_exp = get_exp_cost(tier)
        cost_voidite = get_voidite_cost(tier)
        cumulative_exp += cost_exp
        cumulative_voidite += cost_voidite
        print(f" Tier {tier+1}: {cost_exp:3d} EXP | {cost_voidite:2d} Voidite  (Cumulative: {cumulative_exp:4d} EXP, {cumulative_voidite:2d} Voidite)")
        
    print("-" * 60)
    refund_exp = round(cumulative_exp * 0.85)
    loss_exp = cumulative_exp - refund_exp
    print(f" Total to max 1 branch (5 Tiers): {cumulative_exp} EXP, {cumulative_voidite} Voidite")
    print(f" Respec refund (85%): {refund_exp} EXP (Penalty loss: {loss_exp} EXP)")
    print("-" * 60)
    
    # 6 Total Upgrade Branches (DrillSpeed, DrillDurability, ThrusterTank, KineticDynamo, SonarFreq, ReinforcedPlating)
    total_tree_exp = cumulative_exp * 6
    total_tree_voidite = cumulative_voidite * 6
    print(f" Total to max ALL 6 branches: {total_tree_exp} EXP, {total_tree_voidite} Voidite")
    
    # Average expedition yield
    avg_exp = 350
    runs_to_max_all = math.ceil(total_tree_exp / avg_exp)
    print(f" Estimated expeditions to complete full tree: ~{runs_to_max_all} successful runs")
    print("=" * 60)
    return True

if __name__ == "__main__":
    verify_progression_invariants()
