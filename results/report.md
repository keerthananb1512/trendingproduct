# Daily Heavy-Hitter Report

## Project Summary

This project detects daily trending products from an e-commerce event stream
using Count-Min Sketch.

The input data contains product events in this format:

```text
date,event,itemid
```

Each `itemid` represents a product. For every day, the program streams all
product events into a Count-Min Sketch and reports products whose estimated
frequency crosses the heavy-hitter threshold:

```text
threshold = total_day_events / k
```

For the current run:

```text
k = 100
delta = 0.01
epsilon = 1 / (2k) = 0.005
```

So a product is considered trending on a day if it appears in about 1% or more
of that day's events.

## Output File

The generated result file is:

```text
results/daily_heavy_hitters.csv
```

The validated result file is:

```text
results/daily_heavy_hitters_validated.csv
```

Columns:

```text
date,itemid,estimated_count,exact_count,error,threshold,total_day_events
```

Meaning:

| Column | Meaning |
|---|---|
| `date` | Day being analyzed. |
| `itemid` | Product identifier. |
| `estimated_count` | Approximate frequency estimated by Count-Min Sketch. |
| `exact_count` | Exact frequency computed only for validation. |
| `error` | `estimated_count - exact_count`. |
| `threshold` | Minimum estimated count needed to qualify as a heavy hitter. |
| `total_day_events` | Total number of product events on that date. |

## Exact Count Validation

Count-Min Sketch is used because exact counting every product can be expensive
in a real streaming system. However, for this project we also computed exact
counts offline to validate the approximation.

Validation output:

```text
results/daily_heavy_hitters_validated.csv
```

The validation adds:

```text
exact_count,error
```

where:

```text
error = estimated_count - exact_count
```

For the current run with `k = 100`:

| Metric | Value |
|---|---:|
| CMS reported heavy-hitter rows | 13 |
| Exact true heavy hitters | 7 |
| Exact true heavy hitters missed by CMS | 0 |
| Near-threshold false positives | 6 |
| Total overestimation error across reported rows | 302 |
| Maximum overestimation for one row | 34 |

This is the expected Count-Min Sketch behavior:

- it does not underestimate counts
- it found all exact heavy hitters
- it also reported a few products close to the threshold because hash collisions
  increased their estimated count

Example:

```text
date,itemid,estimated_count,exact_count,error,threshold,total_day_events
2015-05-03,203248,160,141,19,136.83,13683
```

Here product `203248` truly appeared 141 times. Count-Min Sketch estimated 160,
so the overestimation error was 19. Since the threshold was 136.83, this product
is still a true heavy hitter.

## Why Some Days Are Missing

Not every day appears in `daily_heavy_hitters.csv`.

That is expected.

The output file only includes days where at least one product crossed the
heavy-hitter threshold:

```text
estimated_count >= total_day_events / k
```

If a day is missing, it means no product was dominant enough on that day.

If all product activity is spread across many products and no single product
reaches threshold, that day will not have any reported heavy hitters.

## Why Some Days Have Only One Product
In a large e-commerce dataset, most products receive only a small share of daily
traffic. Only unusually active products cross the threshold.

Therefore, it is normal to see:

- no product on some days
- one product on some days
- a few products on unusually concentrated days

This is the behavior of heavy-hitter detection. It is not trying to force a
fixed top 10 list every day.

## How This Is Useful In The Real World

Large e-commerce companies receive huge event streams:

```text
searches, product views, add-to-cart events, purchases
```

Exact counting for every product can become expensive when there are millions of
products and billions of events. Count-Min Sketch helps because it uses a small
fixed-size table and updates counts in streaming fashion.

Real-world uses:

| Use Case | Benefit |
|---|---|
| Homepage trending products | Show products that are unusually popular today. |
| Search trend monitoring | Detect products users are searching/viewing more often. |
| Inventory planning | Alert teams when demand spikes for certain products. |
| Marketing campaigns | Identify products worth promoting while interest is high. |
| Fraud/anomaly detection | Detect unnatural spikes in product views or cart events. |
| Category trend dashboards | Track daily heavy hitters per category. |
| Real-time analytics | Update trend candidates without storing exact counts for every product. |

For example, if a product suddenly becomes a heavy hitter, a company can:

- move it to a trending section
- increase inventory monitoring
- recommend it to similar users
- check whether demand is organic or suspicious
- run targeted ads or discounts

## How Companies Can Choose `k`

The value of `k` controls how strict the trending definition is.

The threshold is:

```text
total_day_events / k
```

So:

| `k` Value | Threshold | Behavior |
|---:|---|---|
| Small `k`, like `50` | Higher threshold | Very strict. Only extremely popular products appear. |
| Medium `k`, like `100` | Moderate threshold | Good for strong daily heavy hitters. |
| Large `k`, like `500` | Lower threshold | More products appear, including weaker trends. |
| Very large `k`, like `1000` | Very low threshold | Useful for exploration, but may include noisy products. |

Example with 100,000 daily events:

| `k` | Threshold | Meaning |
|---:|---:|---|
| `50` | `2,000` | Product must appear at least 2,000 times. |
| `100` | `1,000` | Product must appear at least 1,000 times. |
| `500` | `200` | Product must appear at least 200 times. |
| `1000` | `100` | Product must appear at least 100 times. |

So a company can tune `k` based on business need:

- Use smaller `k` for only the strongest trends.
- Use larger `k` for broader discovery.
- Use different `k` values for different categories.
- Use different `k` values for different event types.

## Choosing `k` By Business Scenario

| Scenario | Suggested `k` Strategy |
|---|---|
| Homepage trending section | Smaller `k`, because only a few strong products should appear. |
| Internal analytics dashboard | Medium `k`, to show meaningful but not too noisy trends. |
| Long-tail product discovery | Larger `k`, to catch weaker emerging products. |
| Fraud/spike detection | Tune `k` per category and monitor sudden threshold crossing. |
| Category-specific trends | Use larger `k` for small categories and smaller `k` for huge categories. |

## Relationship Between `k`, `epsilon`, and Table Size

In this project we use the PDF-style setting:

```text
epsilon = 1 / (2k)
```

Then:

```text
width = ceil(e / epsilon)
rows = ceil(ln(1 / delta))
```

If `k` increases:

- the threshold becomes lower
- more products may qualify
- `epsilon` becomes smaller
- the Count-Min Sketch width becomes larger
- memory use increases

This is the tradeoff:

```text
larger k = more sensitive trend detection + more memory
smaller k = stricter trend detection + less memory
```

## What The Current Run Means

The current result uses:

```text
k = 100
```

This means we are looking for products that received at least about 1% of a
day's product activity.

Because only 13 heavy-hitter rows were found across 139 days, the dataset does
not have many days where one product dominates the full day's activity. That is
not a failure. It means the current `k` is strict and is only catching strong
daily product spikes.

If the business wants more products per day, it can increase `k`.

If the business wants only very dominant products, it can decrease `k`.

## Final Interpretation

This project does not produce a fixed top-N list. It produces an approximate
daily heavy-hitter list.

That means each reported product has enough estimated frequency to cross the
business-defined trend threshold:

```text
total_day_events / k
```

This makes the system scalable, explainable, and useful for large product-based
companies that need fast trend detection from high-volume event streams.
