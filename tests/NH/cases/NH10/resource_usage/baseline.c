volatile unsigned g_resource_value = 123u;

int main(void) {
  return (int)(g_resource_value & 0u);
}
