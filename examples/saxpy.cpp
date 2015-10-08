#include <vector>
#include <cassert>
#include <iostream>
#include <chrono>
#include <agency/execution_policy.hpp>
#include <agency/future.hpp>

void saxpy(size_t n, float a, const float* x, const float* y, float* z)
{
  using namespace agency;

  bulk_invoke(par(n), [=](parallel_agent &self)
  {
    int i = self.index();
    z[i] = a * x[i] + y[i];
  });
}

int main()
{
  size_t n = 1 << 30;
  std::vector<float> x(n, 1), y(n, 2), z(n);
  float a = 13.;

  saxpy(n, a, x.data(), y.data(), z.data());

  std::vector<float> ref(n, a * 1.f + 2.f);
  assert(ref == z);

  std::cout << "Measuring performance..." << std::endl;

  // time a number of trials
  size_t num_trials = 20;

  auto start = std::chrono::high_resolution_clock::now();
  for(size_t i = 0; i < num_trials; ++i)
  {
    saxpy(n, a, x.data(), y.data(), z.data());
  }
  std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;

  double seconds = elapsed.count() / num_trials;
  double gigabytes = double(3 * n * sizeof(float)) / (1 << 30);

  std::cout << "SAXPY Bandwidth: " << gigabytes / seconds << " GB/s" << std::endl;

  return 0;
}
