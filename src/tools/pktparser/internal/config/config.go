package config

type Config struct {
	fileName string
}

func NewConfig() *Config {
	return &Config{}
}

func (c *Config) SetFileName(fileName string) {
	c.fileName = fileName
}

func (c *Config) GetFileName() string {
	return c.fileName
}
