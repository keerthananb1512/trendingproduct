# Daily Heavy-Hitter Report

## Project Summary

This project detects daily trending products from an e-commerce event stream
using Count-Min Sketch.

The input data contains product events in this format:

```text
date,event,itemid
```

Each `itemid` represents a product. For every day, the program converts event
types into intent weights, streams those weighted product events into a
Count-Min Sketch, and reports products whose estimated score crosses the
heavy-hitter threshold:

```text
threshold = total_day_score / k
```

For the current run:

```text
k = 100
delta = 0.01
baseline epsilon = 1 / (2k) = 0.005
stricter epsilon = 1 / (5k) = 0.002
```

So a product is considered trending on a day if it receives about 1% or more of
that day's weighted engagement score.

Current event weights:

```text
view = 1
addtocart = 3
transaction = 5
```

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
date,itemid,estimated_score,exact_score,error,threshold,total_day_score
```

Meaning:

| Column | Meaning |
|---|---|
| `date` | Day being analyzed. |
| `itemid` | Product identifier. |
| `estimated_score` | Approximate weighted score estimated by Count-Min Sketch. |
| `exact_score` | Exact weighted score computed only for validation. |
| `error` | `estimated_score - exact_score`. |
| `threshold` | Minimum estimated score needed to qualify as a heavy hitter. |
| `total_day_score` | Total weighted engagement score on that date. |

## Exact Count Validation

Count-Min Sketch is used because exact counting every product can be expensive
in a real streaming system. However, for this project we also computed exact
weighted scores offline to validate the approximation.

Validation output:

```text
results/daily_heavy_hitters_validated.csv
```

The validation adds:

```text
exact_score,error
```

where:

```text
error = estimated_score - exact_score
```

For the current comparison with `k = 100`:

| Epsilon choice | Width | Counters | Reported rows | True heavy hitters | False positives | Total error | Max error |
|---|---:|---:|---:|---:|---:|---:|---:|
| `1 / (2k)` | 544 | 2,720 | 10 | 6 | 4 | 233 | 35 |
| `1 / (5k)` | 1,360 | 6,800 | 6 | 6 | 0 | 56 | 19 |

This is the expected Count-Min Sketch behavior:

- it does not underestimate weighted scores
- it found all exact heavy hitters
- the looser setting reported a few products close to the threshold because
  hash collisions increased their estimated score
- the stricter setting used more buckets and removed those false positives in
  this dataset

Example:

```text
date,itemid,estimated_score,exact_score,error,threshold,total_day_score
2015-05-24,290250,257,251,6,182.76,18276
```

Here product `290250` truly received 251 weighted engagement points. Count-Min
Sketch estimated 257 in the stricter run, so the overestimation error was 6. Since the threshold
was 182.76, this product is a true weighted heavy hitter.

## Why Some Days Are Missing

Not every day appears in `daily_heavy_hitters.csv`.

That is expected.

The output file only includes days where at least one product crossed the
heavy-hitter threshold:

```text
estimated_score >= total_day_score / k
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
total_day_score / k
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

In this project we compare two settings:

```text
epsilon = 1 / (2k)  baseline, looser bound
epsilon = 1 / (5k)  stricter bound
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

If the epsilon divisor increases from 2 to 5:

```text
epsilon becomes smaller
width becomes larger
memory increases
false positives usually decrease
```

## What The Current Run Means

The current result uses:

```text
k = 100
```

This means we are looking for products that received at least about 1% of a
day's product activity.

With `epsilon = 1 / (2k)`, 10 weighted heavy-hitter rows were found. With the
stricter `epsilon = 1 / (5k)`, 6 weighted heavy-hitter rows were found. The
stricter setting removed the 4 near-threshold false positives in this dataset.
That is not a failure. It shows how the error bound controls the output quality.

If the business wants more products per day, it can increase `k`.

If the business wants only very dominant products, it can decrease `k`.

## Final Interpretation

This project does not produce a fixed top-N list. It produces an approximate
daily heavy-hitter list.

That means each reported product has enough estimated weighted score to cross
the business-defined trend threshold:

```text
total_day_score / k
```

This makes the system scalable, explainable, and useful for large product-based
companies that need fast trend detection from high-volume event streams.
