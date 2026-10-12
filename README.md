# Daily Event-Wise Trending Products

This project uses Count-Min Sketch to find products that receive a significant
share of the daily activity for each e-commerce event:

- `view`
- `addtocart`
- `transaction`

Each event type is processed independently. A product can therefore be trending
in views without being trending in carts or purchases.

## The Rule in One Line

For every date and event type, report a product when:

```text
estimated product count >= number of rows for that event / k
```

In shorter notation:

```text
n = total rows for one event type on one date
threshold = n / k

product is trending when estimated_count >= threshold
```

`n` is calculated separately for views, add-to-cart events, and transactions.
It is also recalculated for every date.

## What “Trending” Means Here

This project uses the heavy-hitter definition of trending. The output contains
every product that meets the `n / k` threshold.

It is not:

- only the single most popular product;
- a fixed top-10 list; or
- a comparison between different event types.

The number of reported products can differ between dates and event types.

## Worked Example

Suppose one date contains the following event totals and `k = 100`:

| Event | Daily rows (`n`) | Threshold (`n / k`) |
|---|---:|---:|
| `view` | 20,000 | 200 |
| `addtocart` | 500 | 5 |
| `transaction` | 100 | 1 |

The decisions are made independently:

- A product with an estimated 240 views is trending in views because
  `240 >= 200`.
- A product with an estimated 4 add-to-cart events is not trending in carts
  because `4 < 5`.
- A product with an estimated 2 transactions is trending in purchases because
  `2 >= 1`.

The threshold can be fractional. For example, if a date has 296 add-to-cart
rows and `k = 100`, the threshold is `2.96`. Since event counts are integers, a
product needs an estimated count of at least 3.

## Input Data

The program reads:

```text
data/processed/events_daily_clean.csv
```

Required columns:

```text
date,event,itemid
```

Example:

```csv
date,event,itemid
2015-06-02,view,355908
2015-06-02,addtocart,5206
2015-06-02,transaction,356475
```

Each row represents one event for one product. Rows with missing required fields
or unknown event types are skipped.

## Processing Flow

For every date, the program:

1. Creates one sketch for views, one for add-to-cart events, and one for
   transactions.
2. Reads each event and increments the matching product count by 1 in the
   matching sketch.
3. Counts the total number of rows for each event type.
4. Calculates a separate `n / k` threshold for each event type.
5. Writes products whose estimated counts meet their corresponding thresholds.

The three streams remain separate throughout this process:

| Input event | Updated sketch | Output file |
|---|---|---|
| `view` | View sketch | `results/most_viewed.csv` |
| `addtocart` | Add-to-cart sketch | `results/most_added_to_cart.csv` |
| `transaction` | Purchase sketch | `results/most_purchased.csv` |

A product is considered for an output only after that product receives the
matching event. For example, a view-only product is not a purchase candidate.

## Output Files

Despite their filenames, these files contain all products meeting the daily
heavy-hitter threshold, not only one maximum product.

Every output uses the same columns:

```text
date,itemid,estimated_count,exact_count,error,threshold,total_day_events
```

| Column | Meaning |
|---|---|
| `date` | Date of the daily window. |
| `itemid` | Product identifier. |
| `estimated_count` | Count estimated by Count-Min Sketch. |
| `exact_count` | Actual count retained for validation. |
| `error` | `estimated_count - exact_count`. |
| `threshold` | Required count, calculated as `total_day_events / k`. |
| `total_day_events` | Number of input rows for this event type and date. |

Example output row:

```csv
date,itemid,estimated_count,exact_count,error,threshold,total_day_events
2015-05-03,401231,6,6,0,2.96,296
```

Interpretation: product `401231` has an estimated add-to-cart count of 6. The
daily threshold is 2.96, so the product qualifies for that event report.

## Why Count-Min Sketch Is Used

Exact counters require an entry for every product. Count-Min Sketch uses a
fixed-size table, making it suitable for large event streams.

For a product event, every sketch row updates one hashed bucket:

```text
for each sketch row i:
    bucket = hash_i(product) mod width
    counter[i][bucket] += 1
```

The estimated product count is the minimum counter across the rows:

```text
estimate(product) = min(counter[i][hash_i(product) mod width])
```

Taking the minimum reduces hash-collision noise. A Count-Min Sketch estimate
can be higher than the exact count, but it will not be lower.

## Approximation and Validation

Hash collisions can create false positives close to the threshold. The program
keeps exact counts alongside the sketches so the approximation can be measured:

```text
error = estimated_count - exact_count
```

Product qualification uses `estimated_count`. The `exact_count` and `error`
columns are included to validate and explain the result.

The sketch dimensions are:

```text
epsilon = 1 / (epsilon_divisor * k)
width   = ceil(e / epsilon)
rows    = ceil(ln(1 / delta))
```

With probability of at least `1 - delta`, the standard Count-Min Sketch bound
for a particular event stream is:

```text
estimated_count(product) <= exact_count(product) + epsilon * n
```

Here, `n` is the number of rows for that event type on that date.

## Build

Compile with a C++17 compiler:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic \
    src/count_min_sketch.cpp src/daily_heavy_hitters.cpp \
    -o daily_heavy_hitters
```

## Run

Run with the default configuration:

```bash
./daily_heavy_hitters
```

The complete command format is:

```bash
./daily_heavy_hitters INPUT_CSV OUTPUT_DIRECTORY K DELTA EPSILON_DIVISOR
```

Example:

```bash
./daily_heavy_hitters \
    data/processed/events_daily_clean.csv \
    results \
    100 \
    0.01 \
    5
```

| Argument | Default | Purpose |
|---|---|---|
| Input CSV | `data/processed/events_daily_clean.csv` | Source event data. |
| Output directory | `results` | Destination for the three reports. |
| `k` | `100` | Sets the heavy-hitter threshold to `n / k`. |
| `delta` | `0.01` | Controls the probability of exceeding the error bound. |
| Epsilon divisor | `5` | Controls sketch width and collision error. |

## Project Files

```text
src/count_min_sketch.hpp        Count-Min Sketch interface
src/count_min_sketch.cpp        Count-Min Sketch implementation
src/daily_heavy_hitters.cpp     Daily event processing and report generation
data/processed/                 Cleaned input data
results/                        Generated event-wise reports
```

## Current Dataset Results

The current reports were generated with:

```text
k = 100
delta = 0.01
epsilon = 1 / (5k)
daily windows = 139
```

| Report | Qualifying rows |
|---|---:|
| Viewed-product heavy hitters | 10 |
| Add-to-cart heavy hitters | 302 |
| Purchased-product heavy hitters | 2,698 |

The purchase report can contain more qualifying products because purchase
streams have smaller daily totals and therefore smaller `n / k` thresholds.
This does not mean purchases are more frequent than views.
