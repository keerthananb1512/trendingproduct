# Daily Trending Product Detection Using Count-Min Sketch

## Project Goal

This project is framed as a scalable trending-product detector for an e-commerce
company such as Amazon, Flipkart, Myntra, or any large product-based platform.
These companies receive millions of user actions every day, including searches,
product views, add-to-cart events, and purchases.

The system answers one practical business question:

> Which products are trending today?

In this dataset, `itemid` represents a product. In a real company, that product
could be something like wireless earbuds, running shoes, sunscreen, or a phone
case. Count-Min Sketch lets us estimate product trend scores efficiently without
storing exact counters for every product in a massive event stream.

## Core Idea

Given a stream with total daily weighted score `n` and a threshold parameter
`k`, find every product whose weighted engagement score is at least:

```text
n / k
```

points.

These products are called heavy hitters. In this project, heavy hitters are the
daily trending products.

Example:

```text
n = 10,000 weighted engagement points in one day
k = 100
threshold = n / k = 100
```

So any product with at least 100 weighted engagement points that day should be
detected as a trending product.

This is not the same as sorting all products and taking a normal top 10 list.

## Dataset

We use the Retailrocket e-commerce event dataset from Kaggle.

Current cleaned input file:

```text
data/processed/events_daily_clean.csv
```

Columns:

```text
date,event,itemid
```

Example:

```text
2015-06-02,view,355908
2015-06-02,addtocart,5206
```

`itemid` is the product identifier. The `event` column is used to weight user
intent:

```text
view = 1
addtocart = 3
transaction = 5
```

## Daily Window Approach

The dataset is processed one day at a time.

For each date:

1. Take only the events from that date.
2. Treat the events as a stream.
3. Convert the event type into a weight.
4. Feed each `itemid` and weight into Count-Min Sketch.
5. Track potential heavy hitters while the stream is processed.
6. At the end of the day, output products whose estimated score is at least
   `n / k`.

This gives a daily trending-product list:

```text
date,itemid,estimated_score,threshold
```

## Count-Min Sketch

Count-Min Sketch is small space probabilistic data structure 

The table has:

```text
rows = number of hash functions
width = number of buckets per row
```

For each weighted product event `x`:

```text
for each row i:
    bucket = hash_i(x) mod width
    CMS[i][bucket] += event_weight
```

To estimate the score of product `x`:

```text
estimate(x) = min(CMS[i][hash_i(x) mod width]) over all rows
```

## Why The Estimate Uses Minimum

So each row gives an estimate like:

```text
row_estimate_i(x) = true_count(x) + collision_error_i
```

The collision error is never negative, because counters only increase. That
means Count-Min Sketch may overestimate a product frequency, but it does not
underestimate it.

Different rows use different hash functions. A product may collide badly in one
row but not in another row. Therefore we take:

```text
estimate(x) = minimum estimate across all rows
```

The minimum chooses the row with the least collision noise. This is why the data
structure is called Count-Min Sketch: it counts with counters, then answers with
the minimum counter value.

## Parameter Choice


```text
width = ceil(e / epsilon)
rows = ceil(ln(1 / delta))
```

Here:

| Parameter | Meaning |
|---|---|
| `epsilon` | Controls how much overestimation error we allow. |
| `delta` | Controls the probability that the estimate is worse than the allowed error. |
| `width` | Number of buckets per row. Larger width means fewer collisions. |
| `rows` | Number of hash functions. More rows means lower failure probability. |

The Count-Min Sketch guarantee is:

```text
estimated_score(x) <= true_score(x) + epsilon * n
```

with probability at least:

```text
1 - delta
```

So if `delta = 0.01`, then the estimate satisfies the error bound with about
99% confidence.

For heavy hitters, I compare two epsilon choices:

```text
baseline: epsilon = 1 / (2k)
stricter: epsilon = 1 / (5k)
```

Why:

- The heavy-hitter threshold is `n / k`.
- The Count-Min Sketch error is controlled around `epsilon * n`.
- Setting `epsilon = 1 / (2k)` makes the error bound half of the threshold.
- Setting `epsilon = 1 / (5k)` makes the error bound one-fifth of the threshold.
- Smaller epsilon means fewer false positives, but it needs more buckets.

This means if the threshold is:

```text
n / k
```

then the possible false-positive region starts around:

```text
n / k - epsilon * n
```

With `epsilon = 1 / (2k)`, that becomes:

```text
n / k - n / (2k) = n / (2k)
```

With `epsilon = 1 / (5k)`, that becomes:

```text
n / k - n / (5k) = 4n / (5k)
```

So the final output is expected to contain:

1. All products whose true frequency is at least `n / k`.
2. Some extra products whose true frequency is close to the threshold.
3. No products that are far below the threshold, except with probability
   controlled by `delta`.

Example with `k = 100` and `delta = 0.01`:

```text
rows = ceil(ln(100)) = 5
```

| Epsilon choice | Error as part of threshold | Width | Table size |
|---|---:|---:|---:|
| `1 / (2k)` | 50% | 544 | 2,720 counters |
| `1 / (5k)` | 20% | 1,360 | 6,800 counters |

Both are still small compared with storing exact counters for all products.

For a day with `n = 10,000` events:

```text
heavy-hitter threshold = n / k = 10,000 / 100 = 100
error bound with 1/(2k) = 50
error bound with 1/(5k) = 20
confidence = 1 - delta = 99%
```

So the output should include all products that appear at least 100 times. It may
also include some products that appear roughly between 50 and 100 times because
their estimated count can be increased by collisions.

## Hash Functions

The C++ implementation will use deterministic seeded hashing.

For row `i`:

```text
bucket = splitmix64(itemid + seed[i]) mod width
```

Why this choice:

- We need one independent-looking hash per row.
- `splitmix64` is fast and simple.
- Fixed seeds make results reproducible.
- It avoids relying on implementation-specific `std::hash` behavior.

## Candidate Heavy-Hitter Tracking

Count-Min Sketch can estimate the count of a product, but it does not store the
complete product list. Therefore, we keep a candidate set of potential heavy
hitters while processing the stream.

For each day:

```text
m = 0
candidate_set = empty

for each itemid x in the day stream:
    m += 1
    update CMS with x
    estimated_score = estimate(x)
    current_threshold = m / k

    if estimated_score >= current_threshold:
        add or update x in candidate_set
```

At the end of the day:

```text
final_threshold = n / k

for each candidate x:
    if estimate(x) >= final_threshold:
        output x as a daily heavy hitter
```

As `m` grows during the day, the threshold grows too. Some early candidates may
not pass the final threshold, so we clean the candidate set at the end.

## What Products Will We Get At The End?

For each day, the final output contains products whose estimated frequency is at
least:

```text
n / k
```

These are the daily trending products.

Because Count-Min Sketch overestimates counts, the output has this meaning:

| Product Type | Output Behavior |
|---|---|
| True count `>= n / k` | Should be included. These are real daily heavy hitters. |
| True count between `n / k - epsilon * n` and `n / k` | May be included. These are near-threshold products. |
| True count far below `n / k - epsilon * n` | Should usually not be included. If included, it is due to unlucky collisions. |

So our result is an approximate daily trending list, not an exact sorted product
leaderboard.

Example with `n = 10,000`, `k = 100`:

| Epsilon choice | True score range that may become false positive |
|---|---|
| `1 / (2k)` | `50` to `99` |
| `1 / (5k)` | `80` to `99` |

The probability of a bad estimate is controlled by `delta`. With
`delta = 0.01`, the failure probability is about 1%.

## Algorithm Summary

For each date:

1. Initialize Count-Min Sketch.
2. Initialize an empty candidate set.
3. Stream that day's events one by one.
4. Convert event type to weight: `view = 1`, `addtocart = 3`, `transaction = 5`.
5. Update the sketch for each `itemid` by that event weight.
6. Estimate the current item's weighted score.
7. If the estimate crosses `m / k`, keep it as a candidate.
8. After the day ends, use final threshold `n / k`.
9. Output candidates whose estimated score is at least `n / k`.

## Expected Output

```text
date,itemid,estimated_score,exact_score,error,threshold,total_day_score
2015-05-24,290250,257,251,6,182.76,18276
```

## Validation

Count-Min Sketch is an approximate algorithm, so we validate its output using
exact counting.

During validation, the program also keeps an exact per-day counter:

```text
exact_score[itemid] += event_weight
```

This exact counter is used only for checking the result. The scalable algorithm
is still Count-Min Sketch.

The validated output file is:

```text
results/daily_heavy_hitters_validated.csv
```

It contains:

```text
date,itemid,estimated_score,exact_score,error,threshold,total_day_score
```

where:

```text
error = estimated_score - exact_score
```

For the current run with `k = 100`, the validation comparison showed:

| Epsilon choice | Width | Counters | Reported rows | True heavy hitters | False positives | Total error | Max error |
|---|---:|---:|---:|---:|---:|---:|---:|
| `1 / (2k)` | 544 | 2,720 | 10 | 6 | 4 | 233 | 35 |
| `1 / (5k)` | 1,360 | 6,800 | 6 | 6 | 0 | 56 | 19 |

This matches the expected Count-Min Sketch behavior: it found all true heavy
hitters in both runs. The stricter epsilon used more memory but reduced
collision error and removed the near-threshold false positives in this dataset.

## Why This Is Useful

In a real e-commerce system, exact counting for every product can become
expensive when the event stream is huge. Count-Min Sketch gives a compact,
streaming-friendly way to detect products that are receiving unusually high
attention each day.

This version uses daily windows and weighted events:

```text
view = 1, addtocart = 3, transaction = 5
```

## Next Implementation Step

Implement the C++ daily heavy-hitter detector:

```text
src/count_min_sketch.hpp
src/count_min_sketch.cpp
src/daily_heavy_hitters.cpp
```

The program will read:

```text
data/processed/events_daily_clean.csv
```

and write:

```text
results/daily_heavy_hitters_validated.csv
```
