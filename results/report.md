# Daily Event-Wise Trending-Product Report

## Method

Views, add-to-cart events, and purchases are processed independently using one
Count-Min Sketch per event type.

For every date and event type:

```text
n = number of rows for that event type on that date
threshold = n / k
```

A product qualifies when:

```text
estimated_count >= threshold
```

## Configuration

```text
k = 100
delta = 0.01
epsilon = 1 / (5 * k)
daily windows = 139
```

## Generated Results

| File | Event stream | Rows |
|---|---|---:|
| `most_viewed.csv` | `view` | 10 |
| `most_added_to_cart.csv` | `addtocart` | 302 |
| `most_purchased.csv` | `transaction` | 2,698 |

## Validation

Each result contains an approximate count and an exact validation count:

```text
error = estimated_count - exact_count
```

| Report | Total collision error | Maximum row error | CMS false positives |
|---|---:|---:|---:|
| Most viewed | 80 | 16 | 1 |
| Most added to cart | 0 | 0 | 0 |
| Most purchased | 0 | 0 | 0 |

The view false positive has an estimated count of `130`, an exact count of
`125`, and a threshold of `127.54`. This is visible in the validation columns.

## Interpretation

These are event-wise heavy hitters, not fixed top-N lists. Purchase streams
have smaller daily row totals, so their `n / k` thresholds are commonly much
lower than view thresholds. This explains why more products can qualify in the
purchase report even though purchases are less frequent.
