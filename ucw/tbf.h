/*
 *	UCW Library -- Rate Limiting based on the Token Bucket Filter
 *
 *	(c) 2009 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_TBF_H_
#define _UCW_TBF_H_

struct token_bucket_filter {
  double rate;				// Number of tokens per second
  uns burst;				// Capacity of the bucket
  timestamp_t last_hit;			// Internal state...
  double bucket;
};

void tbf_init(struct token_bucket_filter *f);
int tbf_limit(struct token_bucket_filter *f, timestamp_t now);

#endif
