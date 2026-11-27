"""Bounded two-placement prototype for the notebook's v5 placement Q-model."""

import numpy as np
import torch


def choose_placement_index(env, model, encode, *, device="cpu", width=6,
                           gamma=0.99):
    """Rank root actions by Q, then refine the top roots with one known piece.

    Only the preview present at the root is available to lookahead. The env is
    restored before returning; reacquire legal placements using the index.
    """
    if width < 1:
        raise ValueError("width must be positive")
    root = env.checkpoint()
    observation = env.observe()
    actions = env.legal_placements()
    if not actions:
        raise ValueError("No legal placements")

    def scores(state, candidates):
        batch = np.stack([encode(state, action) for action in candidates])
        with torch.inference_mode():
            return model(torch.as_tensor(batch, dtype=torch.float32,
                                         device=device)).detach().cpu().numpy()

    was_training = model.training
    model.eval()
    try:
        root_scores = scores(observation, actions).copy()
        known = observation["preview"]
        for index in np.argsort(root_scores)[-width:]:
            env.restore(root)
            candidate = env.legal_placements()[int(index)]
            consumed = 1 + int(candidate.use_hold and observation["hold_piece"] is None)
            next_state, reward, terminal, truncated, _ = env.step(candidate)
            if terminal or truncated:
                root_scores[index] = reward
            elif len(known) >= consumed:
                # A clone can generate unseen preview pieces; never use them
                # as actions or as model inputs.
                next_state = {**next_state, "preview": tuple(known[consumed:])}
                next_actions = env.legal_placements()
                if not next_state["preview"] and next_state["hold_piece"] is None:
                    next_actions = tuple(a for a in next_actions if not a.use_hold)
                if next_actions:
                    root_scores[index] = reward + gamma * max(
                        scores(next_state, next_actions)
                    )
        return int(np.argmax(root_scores))
    finally:
        env.restore(root)
        model.train(was_training)
