arch=${arch} test=experiments/DecodeBatchScale/test_dsv3_fp8_decode_batch_scale_at_ep1.py make llm
arch=${arch} test=experiments/DecodeBatchScale/test_dsv3_fp8_decode_batch_scale_at_ep2.py make llm
arch=${arch} test=experiments/DecodeBatchScale/test_dsv3_fp8_decode_batch_scale_at_ep4.py make llm
arch=${arch} test=experiments/DecodeBatchScale/test_dsv3_fp8_decode_batch_scale_at_ep8.py make llm
arch=${arch} test=experiments/DecodeBatchScale/test_dsv3_fp8_decode_batch_scale_at_ep16.py make llm
arch=${arch} test=experiments/DecodeBatchScale/test_dsv3_fp8_decode_batch_scale_at_ep32.py make llm
arch=${arch} test=experiments/DecodeBatchScale/test_dsv3_fp8_decode_batch_scale_at_ep64.py make llm
