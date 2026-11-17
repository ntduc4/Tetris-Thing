# CS4246 notes for a Tetris AI

Source: the 22 PDFs in `/home/ntd/Documents/[3] Lecture slides/CS4246/`, especially `Course-overview.pdf` (AY2025–26). This is a paraphrased, Tetris-focused study guide, not a replacement for the slides. The tutorial solution PDFs are student submissions; use the lectures to resolve any conflicting notation or answers. The separate [AI plan](AI_PLAN.md) turns these ideas into experiments.

## What is actually in the folder?

| Material | Core idea | Relevance to this project |
| --- | --- | --- |
| `Lecture-Introduction.pdf` | Agents observe, model, plan, learn and act; different planning problems have different assumptions. Responsible decisions and resource limits matter. | Define exactly what an AI can observe and how much thinking time it gets. |
| `Lecture-classic v3.0.pdf`, `Lecture-classic-addendum.pdf`, `Lecture-classic v3.0 Examples.pdf`; Tutorial 1 | STRIPS/PDDL: states, action preconditions/effects, goals; forward/backward search and SATPlan. | Legal moves and reachable placements are the action model; enumerate them before scoring them. A static plan is not enough when future pieces/garbage are uncertain. |
| `Lecture-adv-classic v3.4.pdf`, its addendum, examples and errata; Tutorial 2 | Search heuristics, relaxation, pruning, state abstraction, HTNs/high-level actions; also LLM-assisted planning. The FF heuristic is **not** generally admissible (see errata). | Use board features to rank/prune search, and separate movement paths from placement decisions. A heuristic's practical usefulness is not an optimality proof. |
| `Lecture-rational-utility.pdf`, its addendum and examples/notes; Tutorial 3 | Expected utility, preferences, risk attitudes and a small introductory game-theory example. | Choose what “good Tetris” means: survival, lines, attack, speed and risk are different objectives. In versus play, the opponent affects outcomes. |
| `Lecture-mdp.pdf`, `Lecture-mdp-addendum.pdf`; Tutorial 4 | Markov decision processes, returns/discounting, Bellman equations, value iteration and policy iteration. | Formulate a game step as a transition and reward. The addendum explicitly says exhaustive value/policy iteration is impractical for Tetris; use approximation or decision-time planning. |
| `Lecture-rl.pdf`, `Lecture-rl addendum.pdf`; Tutorial 5 | Passive ADP/Monte Carlo/TD evaluation; active exploration, Monte Carlo control, **SARSA** and **Q-learning**; n-step TD and TD(λ) are also noted. | Provides the actual learning rules and the on-policy/off-policy distinction for the first RL comparison. |
| `Lecture-online-demo.pdf` | Rollouts, MCTS/UCT, behavioral cloning, DAgger, and an AlphaGo-family overview (search + learned policy/value). | A simulator and a MisaMino-like searcher can be baselines or teachers. Search, imitation and RL can also be combined. |

### Overview topics without a corresponding detailed lecture here

`Course-overview.pdf` schedules Week 7 value-function generalization (**DQN**), policy search (**PPO**) and **actor–critic**. The RL lecture only mentions DQN as a historical application; `Lecture-online-demo.pdf` alludes to these methods but does **not** teach their algorithms. Don't infer a full deep-RL lecture from those references. Find a separate DQN/approximate-Q and PPO/actor–critic resource if implementing them.

Likewise, there is **no dedicated** Week 9 lecture on LFM reasoning/RL with human feedback, Week 11 advanced noncooperative game theory, or Week 12 POMDPs in these PDFs. This does not mean zero related content: advanced classical planning includes LLM/PDDL assistance; rational utility introduces a payoff game; some notes mention partial observability; the online/demo lecture **does** explain behavioral cloning and DAgger for the Week 10 imitation theme. IRL and GAIL are named there, not developed into a full IRL or GAIL course. The numbered tutorials in this folder end with introductory Week 7 RL, not deep-RL worked examples.

## Concepts to know before training

### 1. Define the decision problem (MDP and utility)

An MDP is `(states S, available actions A(s), transition T, reward R, discount γ)`. The *Markov* claim applies only if the state contains enough information to predict the next-state distribution: a board alone omits the active piece, queue/hold, combo/B2B, pending garbage, timing and other rule state. Public previews do not reveal arbitrary future draws; under a bag randomizer, track observed history/remaining-bag belief if it matters rather than giving the model the random seed. With limited observations this may be an approximate MDP (or a partial-observation problem), not a guarantee of perfect information.

The return is `G_t = r_t + γ r_(t+1) + γ² r_(t+2) + ...`. `V^π(s) = E[G_t | s, follow π]` values a state; `Q^π(s,a) = E[G_t | s, take a, then follow π]` values an action. For optimal behavior, `V*(s) = max_a Q*(s,a)` and `Q*(s,a) = E[r + γ max_b Q*(s',b) | s,a]`. Policy iteration alternates evaluation and improvement; value iteration repeatedly backs up optimal values. Both enumerate an infeasible number of Tetris boards. Discount choice, terminal loss and reward weights determine what the AI optimizes; **engine score is not automatically the right objective** for survival or versus play.

`Lecture-rational-utility.pdf` motivates maximizing *expected utility*, not just raw average outcome: e.g. a risk-averse versus policy may prefer a safe downstack to a risky high-attack setup. Define the objective and report safety separately rather than silently changing reward functions between algorithms.

### 2. From evaluation to control (RL lecture)

- **Passive evaluation:** follow a fixed policy and estimate its value. ADP learns a transition/reward model and solves Bellman equations; Monte Carlo averages *complete* episode returns (no bootstrap); TD(0) updates after each transition toward `r + γ V(s')` (bootstraps). n-step TD bridges TD(0) and Monte Carlo; TD(λ) combines n-step targets. The Week 7 tutorial works small examples of these updates.
- **Active control:** choose actions while learning. An always-greedy agent can stop discovering good moves; ε-greedy and optimistic exploration trade short-term score for information. In Tetris, sample only **legal** actions. Evaluate without ε-exploration, but train with a declared exploration schedule.
- **SARSA (on-policy Q control):** after choosing the **next action actually to be executed** `a'` from the current exploring policy, target `y = r + γ Q(s',a')` (or `y=r` at terminal). It learns the return of acting with that policy, including exploratory mistakes.
- **Q-learning (off-policy Q control):** target `y = r + γ max_{b∈A(s')} Q(s',b)` (or `y=r` at terminal), even if behavior took another action. It estimates a greedy policy while behavior may explore. The cliff-walking example illustrates why exploratory SARSA can learn a safer path; this is a tendency, **not** a promise it will win at Tetris.

Tutorial 5 makes the target difference concrete: in its toy update `Q(s1,b)` becomes **2.12 with SARSA** using the actually chosen next value `2`, versus **2.44 with Q-learning** using the best next value `4`. This is the first unit test to reproduce for either implementation.

The slides' convergence statements assume the appropriate **tabular**, sufficient-visit and decaying-step/exploration conditions. They **do not** give convergence guarantees for nonlinear PyTorch networks, replay buffers or approximate candidate generation. A Q-network directly learns `Q`; using only a `V`-network for one-step lookahead is a *different* value-learning/planning baseline, not SARSA or Q-learning by another name.
Also distinguish learning from action generation: tabular/standard Q-learning needs no transition model for its TD target, but a *placement-action* agent still needs a way to enumerate which placements are physically reachable. Simulating post-placement boards for the Q input or doing lookahead introduces additional model requirements.

### 3. Search, approximation and expert data

- **Classical/heuristic search:** generate reachable successor boards, evaluate them, keep promising paths. A board heuristic (height, holes, bumpiness, wells, line clears, attack opportunities) can be a strong baseline without being admissible or solving the MDP.
- **Rollout / MCTS:** use a simulator to estimate the future return of each candidate, or use UCT to allocate simulation effort to promising branches. Runtime search trades latency for stronger decisions; it needs correctly modeled moves, rules, previews and chance events. The AlphaGo examples show how policy/value networks can guide search, but aren't a recipe for Tetris without adaptation.
- **Behavioral cloning / DAgger:** train a policy on `(observation, expert legal action)` pairs, then (for DAgger) query the expert on states visited by the learner to reduce distribution shift. MisaMino can supply the expert labels if its actions can be mapped to *this* engine. An imitation learner is not automatically better than its teacher; subsequent RL/search could improve it.

### Reading priorities

1. `Lecture-mdp.pdf` and its Tetris addendum: state, reward, Bellman update, impossibility of exhaustive DP.
2. `Lecture-rl.pdf`, its addendum and Tutorial 5: MC vs TD, action values, SARSA vs Q-learning.
3. `Lecture-online-demo.pdf`: heuristic search/rollouts and optional imitation from MisaMino.
4. Classical/advanced planning and utility: legal-action modeling, heuristics, objective/risk. Treat Week 7 deep-RL details as **external follow-up reading**, not content of these PDFs.
